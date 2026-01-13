// Micro HTTP Server - No dependencies, raw sockets only
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// --- Configuration --- //
static std::string DEVICE_MAC;
static std::string SERVER_IP;
static std::string HOME_API_KEY;
static const int SHUTDOWN_PORT = 10675;
static const int PROBE_PORT = 3389;

const char* get_env(const char* name, const char* default_val) {
    const char* value = std::getenv(name);
    return value ? value : default_val;
}

// --- HTTP Response Helpers --- //
void send_response(int client_fd, int status, const char* status_text, const char* body) {
    char response[1024];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, status_text, strlen(body), body);
    send(client_fd, response, len, 0);
}

inline void send_ok(int fd, const char* body) { send_response(fd, 200, "OK", body); }
inline void send_unauthorized(int fd) { send_response(fd, 401, "Unauthorized", "Unauthorized"); }
inline void send_not_found(int fd) { send_response(fd, 404, "Not Found", "Not Found"); }
inline void send_error(int fd, const char* msg) { send_response(fd, 500, "Internal Server Error", msg); }

// --- HTTP Request Parser --- //
struct HttpRequest {
    std::string method;
    std::string path;
    std::string auth_token;
};

bool parse_request(const char* buffer, HttpRequest& req) {
    // Parse: "GET /path HTTP/1.1\r\n..."
    const char* space1 = strchr(buffer, ' ');
    if (!space1) return false;
    
    req.method = std::string(buffer, space1 - buffer);
    
    const char* space2 = strchr(space1 + 1, ' ');
    if (!space2) return false;
    
    req.path = std::string(space1 + 1, space2 - space1 - 1);
    
    // Parse Authorization header
    const char* auth = strstr(buffer, "Authorization: Bearer ");
    if (auth) {
        auth += 22; // Skip "Authorization: Bearer "
        const char* end = strstr(auth, "\r\n");
        if (end) {
            req.auth_token = std::string(auth, end - auth);
        }
    }
    
    return true;
}

// --- Authentication --- //
bool is_authorized(const HttpRequest& req) {
    if (HOME_API_KEY.empty()) {
        std::cerr << "Warning: HOME_API_KEY is not set!" << std::endl;
        return false;
    }
    return req.auth_token == HOME_API_KEY;
}

// --- Core Logic --- //
bool send_magic_packet(const std::string& mac_address) {
    unsigned char mac_bytes[6];
    int idx = 0;
    
    for (size_t i = 0; i < mac_address.length() && idx < 6; i += 3) {
        mac_bytes[idx++] = (unsigned char)strtol(mac_address.substr(i, 2).c_str(), nullptr, 16);
    }
    
    if (idx != 6) {
        std::cerr << "Invalid MAC address format." << std::endl;
        return false;
    }

    // Build magic packet: 6x 0xFF + 16x MAC
    unsigned char magic_packet[102];
    memset(magic_packet, 0xFF, 6);
    for (int i = 0; i < 16; ++i) {
        memcpy(magic_packet + 6 + (i * 6), mac_bytes, 6);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9);
    addr.sin_addr.s_addr = INADDR_BROADCAST;

    bool ok = sendto(sock, magic_packet, 102, 0, (sockaddr*)&addr, sizeof(addr)) > 0;
    close(sock);
    return ok;
}

bool send_shutdown_command_udp() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SHUTDOWN_PORT);
    inet_pton(AF_INET, SERVER_IP.c_str(), &addr.sin_addr);

    const char* cmd = "shutdown-my-pc";
    bool ok = sendto(sock, cmd, strlen(cmd), 0, (sockaddr*)&addr, sizeof(addr)) > 0;
    close(sock);
    if (ok) std::cout << "UDP shutdown sent." << std::endl;
    return ok;
}

bool send_shutdown_command_tcp() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SHUTDOWN_PORT);
    inet_pton(AF_INET, SERVER_IP.c_str(), &addr.sin_addr);

    // Set connect timeout
    struct timeval tv = {5, 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    const char* cmd = "shutdown-my-pc";
    bool ok = send(sock, cmd, strlen(cmd), 0) > 0;
    close(sock);
    if (ok) std::cout << "TCP shutdown sent." << std::endl;
    return ok;
}

bool send_shutdown_command() {
    bool udp = send_shutdown_command_udp();
    bool tcp = send_shutdown_command_tcp();
    return udp || tcp;
}

bool is_pc_online() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct timeval tv = {1, 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROBE_PORT);
    inet_pton(AF_INET, SERVER_IP.c_str(), &addr.sin_addr);

    bool online = connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0;
    close(sock);
    return online;
}

// --- Request Handler --- //
void handle_request(int client_fd, const HttpRequest& req) {
    std::cout << "Request: " << req.method << " " << req.path << std::endl;

    // Only accept GET
    if (req.method != "GET") {
        send_not_found(client_fd);
        return;
    }

    // Check auth for all endpoints
    if (!is_authorized(req)) {
        send_unauthorized(client_fd);
        std::cerr << "Result: Unauthorized" << std::endl;
        return;
    }

    // Route
    if (req.path == "/turn-on") {
        std::cout << "Action: Magic packet to " << DEVICE_MAC << std::endl;
        if (send_magic_packet(DEVICE_MAC)) {
            send_ok(client_fd, "Magic packet sent.");
            std::cout << "Result: Success" << std::endl;
        } else {
            send_error(client_fd, "Failed to send magic packet.");
            std::cerr << "Result: Failure" << std::endl;
        }
    }
    else if (req.path == "/turn-off") {
        std::cout << "Action: Shutdown to " << SERVER_IP << ":" << SHUTDOWN_PORT << std::endl;
        if (send_shutdown_command()) {
            send_ok(client_fd, "Shutdown command sent.");
            std::cout << "Result: Success" << std::endl;
        } else {
            send_error(client_fd, "Failed to send shutdown command.");
            std::cerr << "Result: Failure" << std::endl;
        }
    }
    else if (req.path == "/is-online") {
        std::cout << "Action: Check " << SERVER_IP << ":" << PROBE_PORT << std::endl;
        bool online = is_pc_online();
        send_ok(client_fd, online ? "true" : "false");
        std::cout << "Result: " << (online ? "online" : "offline") << std::endl;
    }
    else {
        send_not_found(client_fd);
    }
}

// --- Main Server --- //
int main(int argc, char** argv) {
    // Load config
    DEVICE_MAC = get_env("DEVICE_MAC", "00:00:00:00:00:00");
    SERVER_IP = get_env("SERVER_IP", "127.0.0.1");
    HOME_API_KEY = get_env("HOME_API_KEY", "");

    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) port = 8080;
    }

    std::cout << "--- SmartHomePCControl-CPP (Micro) ---" << std::endl;
    std::cout << "  DEVICE_MAC: " << DEVICE_MAC << std::endl;
    std::cout << "  SERVER_IP:  " << SERVER_IP << std::endl;
    std::cout << "  HOME_API_KEY: " << (HOME_API_KEY.empty() ? "NOT SET" : "****") << std::endl;
    std::cout << "--------------------------------------" << std::endl;

    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port " << port << "..." << std::endl;

    // Main loop
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // Read request
        char buffer[4096] = {0};
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (n > 0) {
            HttpRequest req;
            if (parse_request(buffer, req)) {
                handle_request(client_fd, req);
            } else {
                send_not_found(client_fd);
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
