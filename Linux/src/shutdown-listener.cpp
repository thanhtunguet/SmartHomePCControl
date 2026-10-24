#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

// To compile:
// Run: `g++ -O2 -std=c++17 shutdown-listener.cpp -o shutdown-listener`

static int g_signal_pipe[2] = {-1, -1};

static void handle_signal(int sig) {
    char c = static_cast<char>(sig);
    (void)::write(g_signal_pipe[1], &c, 1);
}

void print_help(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [PORT] [--port N] [--message TEXT] [--bind ADDR] [--udp]\n"
        "  PORT            Port to listen on (positional, e.g., from systemd service).\n"
        "  --port N        Port to listen on. Default: 4000\n"
        "  --message TEXT  Activation message. Default: \"shutdown-my-pc\"\n"
        "  --bind ADDR     Bind address (e.g., 127.0.0.1). Default: 0.0.0.0\n"
        "  --udp           Also listen on UDP (default is TCP only).\n"
        "  --help          Show this help.\n";
}

static inline std::string trim(const std::string& s) {
    if (s.empty()) return "";

    auto is_trim_char = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
               c == '\0' || c == '\v' || c == '\f';
    };

    size_t b = 0;
    while (b < s.size() && is_trim_char(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    if (b == s.size()) return "";

    size_t e = s.size() - 1;
    while (e > b && is_trim_char(static_cast<unsigned char>(s[e]))) {
        --e;
    }

    std::string result = s.substr(b, e - b + 1);
    // Truncate at first embedded null byte if client sent a null-terminated string
    size_t null_pos = result.find('\0');
    if (null_pos != std::string::npos) {
        result.resize(null_pos);
        while (!result.empty() && is_trim_char(static_cast<unsigned char>(result.back()))) {
            result.pop_back();
        }
    }
    return result;
}

static inline bool set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void do_poweroff() {
    std::cerr << "[INFO] Activation message received. Initiating poweroff...\n";
    int rc = ::system("systemctl poweroff");
    if (rc != 0) rc = ::system("loginctl poweroff");
    if (rc != 0) rc = ::system("shutdown -h now");
    if (rc != 0) rc = ::system("poweroff");
    if (rc != 0) std::cerr << "[ERROR] poweroff command failed, rc=" << rc << "\n";
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
        else if (a.rfind("--", 0) != 0 && a.rfind("-", 0) != 0) {
            try {
                port = static_cast<uint16_t>(std::stoi(a));
            } catch (...) {
                std::cerr << "Invalid port argument: " << a << "\n";
                print_help(argv[0]);
                return 1;
            }
        }
        else { std::cerr << "Unknown option: " << a << "\n"; print_help(argv[0]); return 1; }
    }

    message = trim(message);

    if (::pipe(g_signal_pipe) < 0) {
        perror("pipe");
        return 1;
    }
    set_nonblock(g_signal_pipe[0]);
    set_nonblock(g_signal_pipe[1]);

    struct sigaction sa{};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN);

    int tcp_fd = make_tcp_listener(bind_addr, port);
    if (tcp_fd < 0) {
        ::close(g_signal_pipe[0]);
        ::close(g_signal_pipe[1]);
        return 2;
    }

    int udp_fd = -1;
    if (enable_udp) {
        udp_fd = make_udp_listener(bind_addr, port);
        if (udp_fd < 0) {
            ::close(tcp_fd);
            ::close(g_signal_pipe[0]);
            ::close(g_signal_pipe[1]);
            return 3;
        }
    }

    std::cerr << "[INFO] Listening on " << bind_addr << ":" << port
              << " (TCP" << (enable_udp ? " + UDP" : "") << "). "
              << "Activation message: \"" << message << "\"\n";

    std::vector<pollfd> pfds;
    pfds.push_back({g_signal_pipe[0], POLLIN, 0});
    pfds.push_back({tcp_fd, POLLIN, 0});
    if (udp_fd >= 0) {
        pfds.push_back({udp_fd, POLLIN, 0});
    }

    std::vector<char> buf(4096);
    bool running = true;

    // Block indefinitely (-1 timeout): 0% CPU, no periodic wake-ups.
    while (running) {
        int rv = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), -1);
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        for (const auto& pfd : pfds) {
            if (!(pfd.revents & POLLIN)) continue;

            // Signal pipe notified (SIGINT / SIGTERM)
            if (pfd.fd == g_signal_pipe[0]) {
                char ch;
                while (::read(g_signal_pipe[0], &ch, 1) > 0) {}
                running = false;
                break;
            }

            // Incoming TCP connection
            if (pfd.fd == tcp_fd) {
                sockaddr_in cli{};
                socklen_t clilen = sizeof(cli);
                int cfd = ::accept(tcp_fd, (sockaddr*)&cli, &clilen);
                if (cfd >= 0) {
                    struct timeval tv{2, 0}; // 2-second receive timeout
                    ::setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                    ssize_t n = ::recv(cfd, buf.data(), buf.size(), 0);
                    if (n > 0) {
                        std::string s(buf.data(), buf.data() + n);
                        if (trim(s) == message) {
                            do_poweroff();
                            running = false;
                        }
                    }
                    ::close(cfd);
                }
                if (!running) break;
            }

            // Incoming UDP datagram
            if (udp_fd >= 0 && pfd.fd == udp_fd) {
                sockaddr_in src{};
                socklen_t srclen = sizeof(src);
                ssize_t n = ::recvfrom(udp_fd, buf.data(), buf.size(), 0, (sockaddr*)&src, &srclen);
                if (n > 0) {
                    std::string s(buf.data(), buf.data() + n);
                    if (trim(s) == message) {
                        do_poweroff();
                        running = false;
                        break;
                    }
                }
            }
        }
    }

    ::close(tcp_fd);
    if (udp_fd >= 0) ::close(udp_fd);
    ::close(g_signal_pipe[0]);
    ::close(g_signal_pipe[1]);

    std::cerr << "[INFO] Exiting gracefully.\n";
    return 0;
}
