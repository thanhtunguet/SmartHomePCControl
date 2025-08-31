#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <atomic>

// To compile:
// Run: `g++ -O2 -std=c++17 -pthread shutdown-listener.cpp -o shutdown-listener`

static std::atomic<bool> g_shutdown_requested{false};
static std::once_flag g_shutdown_once;

void print_help(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [--port N] [--message TEXT] [--bind ADDR] [--udp]\n"
        "  --port N        Port to listen on. Default: 4000\n"
        "  --message TEXT  Activation message. Default: \"shutdown-my-pc\"\n"
        "  --bind ADDR     Bind address (e.g., 127.0.0.1). Default: 0.0.0.0\n"
        "  --udp           Also listen on UDP (default is TCP only).\n"
        "  --help          Show this help.\n";
}

static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static inline bool set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void do_poweroff() {
    std::call_once(g_shutdown_once, [] {
        std::cerr << "[INFO] Activation message received. Initiating poweroff...\n";
        int rc = ::system("systemctl poweroff");
        if (rc != 0) rc = ::system("loginctl poweroff");
        if (rc != 0) rc = ::system("shutdown -h now");
        if (rc != 0) rc = ::system("poweroff");
        if (rc != 0) std::cerr << "[ERROR] poweroff command failed, rc=" << rc << "\n";
        g_shutdown_requested = true;
    });
}

int make_tcp_listener(const std::string& bind_addr, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket TCP"); return -1; }
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bind_addr.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind address: " << bind_addr << "\n";
        ::close(fd);
        return -1;
    }
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind TCP"); ::close(fd); return -1; }
    if (!set_nonblock(fd)) { perror("fcntl O_NONBLOCK TCP"); ::close(fd); return -1; }
    if (::listen(fd, 16) < 0) { perror("listen"); ::close(fd); return -1; }
    return fd;
}

int make_udp_listener(const std::string& bind_addr, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket UDP"); return -1; }
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bind_addr.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind address: " << bind_addr << "\n";
        ::close(fd);
        return -1;
    }
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind UDP"); ::close(fd); return -1; }
    if (!set_nonblock(fd)) { perror("fcntl O_NONBLOCK UDP"); ::close(fd); return -1; }
    return fd;
}

void tcp_worker(int listen_fd, std::string message) {
    std::vector<char> buf(4096);
    while (!g_shutdown_requested.load()) {
        pollfd pfd{listen_fd, POLLIN, 0};
        int rv = ::poll(&pfd, 1, 200); // 200ms tick to check shutdown flag
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("poll TCP");
            break;
        }
        if (rv == 0) continue; // timeout

        if (pfd.revents & POLLIN) {
            sockaddr_in cli{}; socklen_t clilen = sizeof(cli);
            int cfd = ::accept(listen_fd, (sockaddr*)&cli, &clilen);
            if (cfd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                if (errno == EINTR) continue;
                perror("accept");
                continue;
            }
            // Non-block the accepted socket too
            set_nonblock(cfd);
            ssize_t n = ::recv(cfd, buf.data(), buf.size(), 0);
            if (n > 0) {
                std::string s(buf.data(), buf.data() + n);
                if (trim(s) == message) do_poweroff();
            }
            ::close(cfd);
        }
    }
}

void udp_worker(int udp_fd, std::string message) {
    std::vector<char> buf(4096);
    while (!g_shutdown_requested.load()) {
        pollfd pfd{udp_fd, POLLIN, 0};
        int rv = ::poll(&pfd, 1, 200);
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("poll UDP");
            break;
        }
        if (rv == 0) continue;

        if (pfd.revents & POLLIN) {
            sockaddr_in src{}; socklen_t srclen = sizeof(src);
            ssize_t n = ::recvfrom(udp_fd, buf.data(), buf.size(), 0, (sockaddr*)&src, &srclen);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
                perror("recvfrom");
                continue;
            }
            std::string s(buf.data(), buf.data() + n);
            if (trim(s) == message) do_poweroff();
        }
    }
}

void handle_signal(int) {
    g_shutdown_requested = true;
}

int main(int argc, char** argv) {
    uint16_t port = 4000;
    std::string message = "shutdown-my-pc";
    std::string bind_addr = "0.0.0.0";
    bool enable_udp = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { print_help(argv[0]); return 0; }
        else if (a == "--port" && i + 1 < argc) { port = static_cast<uint16_t>(std::stoi(argv[++i])); }
        else if (a == "--message" && i + 1 < argc) { message = argv[++i]; }
        else if (a == "--bind" && i + 1 < argc) { bind_addr = argv[++i]; }
        else if (a == "--udp") { enable_udp = true; }
        else { std::cerr << "Unknown option: " << a << "\n"; print_help(argv[0]); return 1; }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    int tcp_fd = make_tcp_listener(bind_addr, port);
    if (tcp_fd < 0) return 2;

    int udp_fd = -1;
    if (enable_udp) {
        udp_fd = make_udp_listener(bind_addr, port);
        if (udp_fd < 0) { ::close(tcp_fd); return 3; }
    }

    std::cerr << "[INFO] Listening on " << bind_addr << ":" << port
              << " (TCP" << (enable_udp ? " + UDP" : "") << "). "
              << "Activation message: \"" << message << "\"\n";

    std::thread t_tcp(tcp_worker, tcp_fd, message);
    std::thread t_udp;
    if (enable_udp) t_udp = std::thread(udp_worker, udp_fd, message);

    // Main thread: wait until signaled
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Trigger workers to exit and close sockets to unblock any lingering I/O
    ::close(tcp_fd);
    if (udp_fd >= 0) ::close(udp_fd);

    if (t_tcp.joinable()) t_tcp.join();
    if (enable_udp && t_udp.joinable()) t_udp.join();

    std::cerr << "[INFO] Exiting gracefully.\n";
    return 0;
}
