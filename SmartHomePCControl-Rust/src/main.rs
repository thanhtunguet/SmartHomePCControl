use axum::{
    extract::State,
    http::{Request, StatusCode},
    middleware::{self, Next},
    response::{IntoResponse, Response},
    routing::get,
    Router,
};
use std::net::{IpAddr, Ipv4Addr, SocketAddr, TcpStream, UdpSocket};
use std::sync::Arc;
use std::time::Duration;
use tracing::{error, info, warn};

// --- Concurrency & Rate Limiting Constants --- //
const MAX_CONCURRENT_REQUESTS: usize = 5;       // Tối đa 5 request xử lý đồng thời, vượt quá sẽ đợi (await/block)
const MAX_REQUESTS_PER_SECOND: f64 = 5.0;       // Tối đa 5 requests/s cho toàn bộ client

// --- Rate Limiter (Token Bucket) --- //
struct RateLimiter {
    tokens: f64,
    capacity: f64,
    refill_rate: f64,
    last_check: std::time::Instant,
}

impl RateLimiter {
    fn new(max_reqs_per_sec: f64) -> Self {
        Self {
            tokens: max_reqs_per_sec,
            capacity: max_reqs_per_sec,
            refill_rate: max_reqs_per_sec,
            last_check: std::time::Instant::now(),
        }
    }

    fn try_acquire(&mut self) -> bool {
        let now = std::time::Instant::now();
        let elapsed = (now - self.last_check).as_secs_f64();
        self.last_check = now;

        self.tokens += elapsed * self.refill_rate;
        if self.tokens > self.capacity {
            self.tokens = self.capacity;
        }

        if self.tokens >= 1.0 {
            self.tokens -= 1.0;
            true
        } else {
            false
        }
    }
}

// --- Configuration --- //

struct AppConfig {
    device_mac: String,
    server_ip: String,
    web_api_key: String,
    rate_limiter: std::sync::Mutex<RateLimiter>,
    concurrency_semaphore: tokio::sync::Semaphore,
}

impl AppConfig {
    fn from_env() -> Self {
        let device_mac = std::env::var("DEVICE_MAC").unwrap_or_else(|_| "00:00:00:00:00:00".to_string());
        let server_ip = std::env::var("SERVER_IP").unwrap_or_else(|_| "127.0.0.1".to_string());
        let web_api_key = std::env::var("WEB_API_KEY")
            .or_else(|_| std::env::var("HOME_API_KEY"))
            .unwrap_or_default();
        
        Self {
            device_mac,
            server_ip,
            web_api_key,
            rate_limiter: std::sync::Mutex::new(RateLimiter::new(MAX_REQUESTS_PER_SECOND)),
            concurrency_semaphore: tokio::sync::Semaphore::new(MAX_CONCURRENT_REQUESTS),
        }
    }
}

// --- Middlewares --- //

async fn rate_limit_middleware(
    State(config): State<Arc<AppConfig>>,
    request: Request<axum::body::Body>,
    next: Next,
) -> Result<Response, StatusCode> {
    let allowed = {
        let mut limiter = config.rate_limiter.lock().unwrap_or_else(|p| p.into_inner());
        limiter.try_acquire()
    };

    if !allowed {
        warn!("Rate limit exceeded (max {} req/s). Returning 429 Too Many Requests.", MAX_REQUESTS_PER_SECOND);
        return Err(StatusCode::TOO_MANY_REQUESTS);
    }

    Ok(next.run(request).await)
}

async fn concurrency_middleware(
    State(config): State<Arc<AppConfig>>,
    request: Request<axum::body::Body>,
    next: Next,
) -> Result<Response, StatusCode> {
    // Acquire permit: waits (awaits/blocks) if MAX_CONCURRENT_REQUESTS permits are currently held
    let _permit = config
        .concurrency_semaphore
        .acquire()
        .await
        .map_err(|_| StatusCode::INTERNAL_SERVER_ERROR)?;

    Ok(next.run(request).await)
}

async fn auth_middleware(
    State(config): State<Arc<AppConfig>>,
    request: Request<axum::body::Body>,
    next: Next,
) -> Result<Response, StatusCode> {
    if config.web_api_key.is_empty() {
        warn!("WEB_API_KEY is not set!");
        return Err(StatusCode::UNAUTHORIZED);
    }

    let auth_header = request
        .headers()
        .get("Authorization")
        .and_then(|v| v.to_str().ok());

    match auth_header {
        Some(value) => {
            let token = value
                .strip_prefix("Bearer ")
                .or_else(|| value.strip_prefix("bearer "));
            match token {
                Some(token) if token.trim() == config.web_api_key => Ok(next.run(request).await),
                Some(_) => {
                    error!("Invalid API key provided");
                    Err(StatusCode::UNAUTHORIZED)
                }
                None => {
                    error!("Missing Bearer scheme in Authorization header");
                    Err(StatusCode::UNAUTHORIZED)
                }
            }
        }
        _ => {
            error!("Missing or invalid Authorization header");
            Err(StatusCode::UNAUTHORIZED)
        }
    }
}

// --- Core Logic --- //

fn send_magic_packet(mac_address: &str) -> Result<(), String> {
    info!("Attempting to send magic packet to {}", mac_address);
    
    let mac = mac_address.replace(":", "").replace("-", "");
    if mac.len() != 12 {
        return Err("Invalid MAC address format".to_string());
    }

    let mut mac_bytes = [0u8; 6];
    for i in 0..6 {
        mac_bytes[i] = u8::from_str_radix(&mac[i * 2..i * 2 + 2], 16)
            .map_err(|e| format!("Failed to parse MAC address: {}", e))?;
    }

    // Build magic packet: 6 bytes of 0xFF followed by 16 repetitions of MAC address
    let mut magic_packet = vec![0xFF; 6];
    for _ in 0..16 {
        magic_packet.extend_from_slice(&mac_bytes);
    }

    let broadcast_addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::BROADCAST), 9);

    let socket = UdpSocket::bind("0.0.0.0:0")
        .map_err(|e| format!("Failed to create socket: {}", e))?;
    
    socket
        .set_broadcast(true)
        .map_err(|e| format!("Failed to set broadcast: {}", e))?;

    socket
        .send_to(&magic_packet, broadcast_addr)
        .map_err(|e| format!("Failed to send magic packet: {}", e))?;

    info!("Magic packet sent successfully");
    Ok(())
}

fn send_shutdown_command_udp(server_ip: &str, port: u16) -> Result<(), String> {
    let socket = UdpSocket::bind("0.0.0.0:0")
        .map_err(|e| format!("Failed to create UDP socket: {}", e))?;

    let server_addr = format!("{}:{}", server_ip, port);
    let command = b"shutdown-my-pc";

    socket
        .send_to(command, &server_addr)
        .map_err(|e| format!("Failed to send UDP shutdown command: {}", e))?;

    info!("UDP shutdown command sent successfully");
    Ok(())
}

fn send_shutdown_command_tcp(server_ip: &str, port: u16) -> Result<(), String> {
    use std::io::Write;

    let server_addr = format!("{}:{}", server_ip, port);
    let mut stream = TcpStream::connect_timeout(
        &server_addr.parse::<SocketAddr>()
            .map_err(|e| format!("Invalid server address: {}", e))?,
        Duration::from_secs(5),
    )
    .map_err(|e| format!("Failed to connect TCP: {}", e))?;

    let command = b"shutdown-my-pc";
    stream
        .write_all(command)
        .map_err(|e| format!("Failed to send TCP shutdown command: {}", e))?;

    info!("TCP shutdown command sent successfully");
    Ok(())
}

fn send_shutdown_command(server_ip: &str) -> Result<(), String> {
    const SHUTDOWN_PORT: u16 = 10675;

    let udp_result = send_shutdown_command_udp(server_ip, SHUTDOWN_PORT);
    let tcp_result = send_shutdown_command_tcp(server_ip, SHUTDOWN_PORT);

    // Return success if at least one method succeeds
    match (udp_result, tcp_result) {
        (Ok(_), _) | (_, Ok(_)) => Ok(()),
        (Err(e1), Err(e2)) => Err(format!("Both UDP and TCP failed. UDP: {}, TCP: {}", e1, e2)),
    }
}

fn is_pc_online(server_ip: &str) -> bool {
    const PROBE_PORT: u16 = 3389; // RDP port

    let server_addr = format!("{}:{}", server_ip, PROBE_PORT);
    
    match server_addr.parse::<SocketAddr>() {
        Ok(addr) => {
            TcpStream::connect_timeout(&addr, Duration::from_secs(1)).is_ok()
        }
        Err(_) => false,
    }
}

// --- HTTP Handlers --- //

async fn turn_on_handler(State(config): State<Arc<AppConfig>>) -> impl IntoResponse {
    info!("Action: Attempting to send magic packet to {}", config.device_mac);
    
    match send_magic_packet(&config.device_mac) {
        Ok(_) => {
            info!("Result: Success");
            (StatusCode::OK, "Magic packet sent.")
        }
        Err(e) => {
            error!("Result: Failure - {}", e);
            (StatusCode::INTERNAL_SERVER_ERROR, "Failed to send magic packet.")
        }
    }
}

async fn turn_off_handler(State(config): State<Arc<AppConfig>>) -> impl IntoResponse {
    info!("Action: Attempting to send shutdown command to {}:10675", config.server_ip);
    
    match send_shutdown_command(&config.server_ip) {
        Ok(_) => {
            info!("Result: Success");
            (StatusCode::OK, "Shutdown command sent.")
        }
        Err(e) => {
            error!("Result: Failure - {}", e);
            (StatusCode::INTERNAL_SERVER_ERROR, "Failed to send shutdown command.")
        }
    }
}

async fn is_online_handler(State(config): State<Arc<AppConfig>>) -> impl IntoResponse {
    info!("Action: Checking online status for {}:3389", config.server_ip);
    
    let online = is_pc_online(&config.server_ip);
    let response = if online { "true" } else { "false" };
    
    info!("Result: PC is {}", if online { "online" } else { "offline" });
    (StatusCode::OK, response)
}

// --- Main Application --- //

#[tokio::main(flavor = "current_thread")]
async fn main() {
    // Initialize tracing
    tracing_subscriber::fmt()
        .with_target(false)
        .compact()
        .init();

    let config = Arc::new(AppConfig::from_env());

    // Print configuration
    println!("--- SmartHomePCControl-Rust ---");
    println!("Configuration:");
    println!("  - DEVICE_MAC:              {}", config.device_mac);
    println!("  - SERVER_IP:               {}", config.server_ip);
    println!("  - WEB_API_KEY:             {}", if config.web_api_key.is_empty() { "NOT SET" } else { "****" });
    println!("  - MAX_CONCURRENT_REQUESTS: {}", MAX_CONCURRENT_REQUESTS);
    println!("  - MAX_REQUESTS_PER_SECOND: {}", MAX_REQUESTS_PER_SECOND);
    println!("------------------------------");

    // Build the application routes
    // Order of execution for incoming requests:
    // 1. rate_limit_middleware (outermost: reject burst spam with 429)
    // 2. concurrency_middleware (wait if 5 requests are active)
    // 3. auth_middleware (authenticate Bearer token)
    let app = Router::new()
        .route("/api/turn-on", get(turn_on_handler))
        .route("/api/turn-off", get(turn_off_handler))
        .route("/api/is-online", get(is_online_handler))
        .route_layer(middleware::from_fn_with_state(
            config.clone(),
            auth_middleware,
        ))
        .route_layer(middleware::from_fn_with_state(
            config.clone(),
            concurrency_middleware,
        ))
        .route_layer(middleware::from_fn_with_state(
            config.clone(),
            rate_limit_middleware,
        ))
        .with_state(config);

    // Get port from command line arguments or use default
    let port: u16 = std::env::args()
        .nth(1)
        .and_then(|p| p.parse().ok())
        .unwrap_or(8080);

    let addr = SocketAddr::from(([0, 0, 0, 0], port));
    
    println!("Starting server on port {}...", port);
    info!("Server listening on {}", addr);

    // Create TCP listener
    let listener = tokio::net::TcpListener::bind(addr)
        .await
        .unwrap_or_else(|e| {
            eprintln!("Failed to bind to port {}. Is it already in use? Error: {}", port, e);
            std::process::exit(1);
        });

    // Run the server
    axum::serve(listener, app)
        .await
        .unwrap_or_else(|e| {
            eprintln!("Server error: {}", e);
            std::process::exit(1);
        });
}
