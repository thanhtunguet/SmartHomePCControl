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

// --- Configuration --- //

#[derive(Clone)]
struct AppConfig {
    device_mac: String,
    server_ip: String,
    home_api_key: String,
}

impl AppConfig {
    fn from_env() -> Self {
        let device_mac = std::env::var("DEVICE_MAC").unwrap_or_else(|_| "00:00:00:00:00:00".to_string());
        let server_ip = std::env::var("SERVER_IP").unwrap_or_else(|_| "127.0.0.1".to_string());
        let home_api_key = std::env::var("HOME_API_KEY").unwrap_or_default();
        
        Self {
            device_mac,
            server_ip,
            home_api_key,
        }
    }
}

// --- Authentication Middleware --- //

async fn auth_middleware(
    State(config): State<Arc<AppConfig>>,
    request: Request<axum::body::Body>,
    next: Next,
) -> Result<Response, StatusCode> {
    if config.home_api_key.is_empty() {
        warn!("HOME_API_KEY is not set!");
        return Err(StatusCode::UNAUTHORIZED);
    }

    let auth_header = request
        .headers()
        .get("Authorization")
        .and_then(|v| v.to_str().ok());

    match auth_header {
        Some(value) if value.starts_with("Bearer ") => {
            let token = &value[7..];
            if token == config.home_api_key {
                Ok(next.run(request).await)
            } else {
                error!("Invalid API key provided");
                Err(StatusCode::UNAUTHORIZED)
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
    println!("  - DEVICE_MAC: {}", config.device_mac);
    println!("  - SERVER_IP:  {}", config.server_ip);
    println!("  - HOME_API_KEY: {}", if config.home_api_key.is_empty() { "NOT SET" } else { "****" });
    println!("------------------------------");

    // Build the application routes
    let app = Router::new()
        .route("/turn-on", get(turn_on_handler))
        .route("/turn-off", get(turn_off_handler))
        .route("/is-online", get(is_online_handler))
        .route_layer(middleware::from_fn_with_state(
            config.clone(),
            auth_middleware,
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
