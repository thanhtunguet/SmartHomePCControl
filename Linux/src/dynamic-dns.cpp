#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <curl/curl.h>

/**
 * C++ Equivalent of dynamic-dns.sh
 *
 * Compares current public IP with domain DNS record and updates Cloudflare if changed.
 * Reads configuration from environment variables.
 *
 * Environment variables:
 *   - CF_API_KEY           : Cloudflare API Token or Global API Key (required)
 *   - CF_EMAIL             : Cloudflare Account Email (optional; if omitted, uses Bearer Auth)
 *   - ZONE_ID              : Cloudflare Zone ID (required)
 *   - RECORD_ID            : Cloudflare DNS Record ID (required)
 *   - BOT_TOKEN            : Telegram Bot Token (optional)
 *   - CHAT_ID              : Telegram Chat ID (optional)
 *   - YOUR_DOMAIN          : Domain or subdomain to check (e.g. *.example.com) (required)
 *   - DNS_SERVER           : DNS Server to resolve domain (default: 8.8.8.8)
 *   - RECORD_NAME          : Name for Cloudflare DNS record (default: '*' if wildcard, else YOUR_DOMAIN)
 *   - CHECK_IP_URL         : URL to retrieve public IP (default: https://checkip.amazonaws.com)
 *   - CF_PROXIED           : Set proxy status in Cloudflare: "true" or "false" (default: false)
 *   - CF_TTL               : DNS record TTL in seconds or 1 for auto (default: 1)
 *   - CHECK_INTERVAL       : Seconds between checks if running in loop mode (default: 0 = run once)
 */

struct HttpResponse {
    long status_code = 0;
    std::string body;
    std::string error;
};

// Helper: Trim whitespace and newlines
static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Helper: Load key-value pairs from an env file (e.g., ~/.ddns)
static void load_env_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Support lines starting with "export KEY=VAL"
        if (line.rfind("export ", 0) == 0) {
            line = trim(line.substr(7));
        }

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq_pos));
        std::string val = trim(line.substr(eq_pos + 1));

        // Strip enclosing quotes if present
        if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                                (val.front() == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        if (!key.empty()) {
            setenv(key.c_str(), val.c_str(), 0); // 0 = do not overwrite existing env
        }
    }
}

// Helper: Get environment variable with fallback
static inline std::string get_env(const std::string& key, const std::string& default_val = "") {
    const char* val = std::getenv(key.c_str());
    if (val != nullptr && *val != '\0') {
        return std::string(val);
    }
    return default_val;
}

// Helper: Check multiple possible environment variable names
static inline std::string get_env_multi(const std::vector<std::string>& keys, const std::string& default_val = "") {
    for (const auto& key : keys) {
        std::string val = get_env(key);
        if (!val.empty()) {
            return val;
        }
    }
    return default_val;
}

// Helper: JSON escape string
static std::string json_escape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1f) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

// Callback for libcurl to write response data into std::string
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Perform HTTP request using libcurl
static HttpResponse perform_http_request(
    const std::string& method,
    const std::string& url,
    const std::string& payload = "",
    const std::vector<std::string>& headers = {}
) {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }

    struct curl_slist* chunk = nullptr;
    for (const auto& h : headers) {
        chunk = curl_slist_append(chunk, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!payload.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        }
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (!payload.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        }
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    }

    if (chunk) {
        curl_slist_free_all(chunk);
    }
    curl_easy_cleanup(curl);
    return response;
}

// Fetch current public IP
static std::string get_public_ip(const std::string& check_ip_url) {
    HttpResponse resp = perform_http_request("GET", check_ip_url);
    if (resp.status_code == 200 && resp.error.empty()) {
        std::string ip = trim(resp.body);
        struct in_addr addr;
        if (inet_pton(AF_INET, ip.c_str(), &addr) == 1) {
            return ip;
        }
    }
    std::cerr << "[ERROR] Failed to obtain valid public IP from " << check_ip_url
              << (resp.error.empty() ? "" : " (" + resp.error + ")") << std::endl;
    return "";
}

// Resolve domain IP using nslookup against a specific DNS server
static std::string resolve_domain_ip(const std::string& domain, const std::string& dns_server) {
    // 1. Try nslookup with DNS_SERVER
    std::string cmd = "nslookup " + domain;
    if (!dns_server.empty()) {
        cmd += " " + dns_server;
    }
    cmd += " 2>/dev/null";

    std::array<char, 256> buffer;
    std::string ns_output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            ns_output += buffer.data();
        }
        pclose(pipe);
    }

    // Parse nslookup output
    std::istringstream iss(ns_output);
    std::string line;
    bool in_answer_section = false;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::string lower = line;
        for (char& c : lower) c = static_cast<char>(std::tolower(c));

        if (lower.find("answer:") != std::string::npos || lower.find("name:") != std::string::npos) {
            in_answer_section = true;
        }

        if (in_answer_section && lower.find("address") != std::string::npos) {
            std::istringstream ls(line);
            std::string token;
            while (ls >> token) {
                size_t hash_pos = token.find('#');
                if (hash_pos != std::string::npos) {
                    token = token.substr(0, hash_pos);
                }
                struct in_addr addr;
                if (inet_pton(AF_INET, token.c_str(), &addr) == 1) {
                    if (token != dns_server) {
                        return token;
                    }
                }
            }
        }
    }

    // 2. Fallback: getaddrinfo using system resolver
    std::string query_domain = domain;
    if (query_domain.rfind("*.", 0) == 0) {
        query_domain = query_domain.substr(2);
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(query_domain.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
        char ip_str[INET_ADDRSTRLEN];
        auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
        freeaddrinfo(res);
        return std::string(ip_str);
    }

    return "";
}

// Send Telegram notification
static bool send_telegram_message(
    const std::string& bot_token,
    const std::string& chat_id,
    const std::string& message
) {
    if (bot_token.empty() || chat_id.empty()) {
        std::cout << "[INFO] Telegram notification skipped (BOT_TOKEN or CHAT_ID not set)." << std::endl;
        return false;
    }

    std::string url = "https://api.telegram.org/bot" + bot_token + "/sendMessage";
    std::string payload = "{\"chat_id\":\"" + json_escape(chat_id) + "\",\"text\":\"" + json_escape(message) + "\"}";
    std::vector<std::string> headers = {
        "Accept: application/json",
        "Content-Type: application/json"
    };

    HttpResponse resp = perform_http_request("POST", url, payload, headers);
    if (resp.status_code == 200) {
        std::cout << "[INFO] Telegram notification sent successfully." << std::endl;
        return true;
    } else {
        std::cerr << "[ERROR] Failed to send Telegram notification: HTTP "
                  << resp.status_code << " " << resp.body << std::endl;
        return false;
    }
}

// Update Cloudflare DNS record
static bool update_cloudflare_dns(
    const std::string& cf_api_key,
    const std::string& cf_email,
    const std::string& zone_id,
    const std::string& record_id,
    const std::string& record_name,
    const std::string& public_ip,
    int ttl,
    bool proxied
) {
    std::string url = "https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records/" + record_id;

    std::vector<std::string> headers = {
        "Accept: application/json",
        "Content-Type: application/json"
    };

    if (cf_email.empty()) {
        headers.push_back("Authorization: Bearer " + cf_api_key);
    } else {
        headers.push_back("X-Auth-Email: " + cf_email);
        headers.push_back("X-Auth-Key: " + cf_api_key);
    }

    std::string payload = "{\"type\":\"A\",\"name\":\"" + json_escape(record_name) +
                          "\",\"content\":\"" + json_escape(public_ip) +
                          "\",\"ttl\":" + std::to_string(ttl) +
                          ",\"proxied\":" + (proxied ? "true" : "false") + "}";

    HttpResponse resp = perform_http_request("PUT", url, payload, headers);

    if (resp.status_code == 200) {
        std::cout << "[INFO] Cloudflare DNS record updated successfully to " << public_ip << std::endl;
        return true;
    } else {
        std::cerr << "[ERROR] Failed to update Cloudflare DNS: HTTP "
                  << resp.status_code << " " << resp.body << std::endl;
        return false;
    }
}

static void run_ddns_check() {
    // 1. Load configuration from environment variables
    std::string cf_api_key   = get_env_multi({"CF_API_KEY", "CLOUDFLARE_API_KEY", "CLOUDFLARE_API_TOKEN"});
    std::string cf_email     = get_env_multi({"CF_EMAIL", "CLOUDFLARE_EMAIL"});
    std::string zone_id      = get_env_multi({"ZONE_ID", "CF_ZONE_ID"});
    std::string record_id    = get_env_multi({"RECORD_ID", "CF_RECORD_ID"});

    std::string bot_token    = get_env_multi({"BOT_TOKEN", "TELEGRAM_BOT_TOKEN"});
    std::string chat_id      = get_env_multi({"CHAT_ID", "TELEGRAM_CHAT_ID"});

    std::string your_domain  = get_env_multi({"YOUR_DOMAIN", "DOMAIN"});
    std::string dns_server   = get_env("DNS_SERVER", "8.8.8.8");

    std::string record_name  = get_env_multi({"RECORD_NAME", "CF_RECORD_NAME"});
    if (record_name.empty()) {
        // If domain is wildcard "*.example.com", default record name is "*"
        if (your_domain.rfind("*.", 0) == 0) {
            record_name = "*";
        } else {
            record_name = your_domain;
        }
    }

    std::string check_ip_url = get_env("CHECK_IP_URL", "https://checkip.amazonaws.com");
    bool proxied             = (get_env("CF_PROXIED", "false") == "true");
    int ttl                  = std::atoi(get_env("CF_TTL", "1").c_str());
    if (ttl <= 0) ttl = 1;

    // Validate required configuration
    if (cf_api_key.empty() || zone_id.empty() || record_id.empty() || your_domain.empty()) {
        std::cerr << "[ERROR] Missing required environment variables:\n";
        if (cf_api_key.empty())  std::cerr << "  - CF_API_KEY\n";
        if (zone_id.empty())     std::cerr << "  - ZONE_ID\n";
        if (record_id.empty())   std::cerr << "  - RECORD_ID\n";
        if (your_domain.empty()) std::cerr << "  - YOUR_DOMAIN\n";
        return;
    }

    // 2. Get current public IP address of this network
    std::string public_ip = get_public_ip(check_ip_url);
    if (public_ip.empty()) {
        std::cerr << "[ERROR] Could not determine current public IP." << std::endl;
        return;
    }

    // 3. Get current DNS setting of the domain
    std::string current_ip = resolve_domain_ip(your_domain, dns_server);

    // 4. Compare the two IP addresses
    if (current_ip != public_ip) {
        std::string notify_msg = "The public IP address has changed from: " +
                                 (current_ip.empty() ? "(unknown)" : current_ip) +
                                 " to " + public_ip + ". DNS setting has been updated!";
        std::cout << "[INFO] " << notify_msg << std::endl;

        // Send Telegram notification
        send_telegram_message(bot_token, chat_id, notify_msg);

        // Update Cloudflare DNS record
        update_cloudflare_dns(cf_api_key, cf_email, zone_id, record_id, record_name, public_ip, ttl, proxied);
    } else {
        std::cout << "Current DNS setting is up to date" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // 0. Auto-load configuration from env file (e.g. ~/.ddns)
    std::string env_file_path = get_env("DDNS_ENV_FILE");
    int interval = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--loop" && i + 1 < argc) {
            interval = std::atoi(argv[++i]);
        } else if ((arg == "--env" || arg == "-e") && i + 1 < argc) {
            env_file_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [--env <path>] [--loop <seconds>]\n"
                      << "  --env, -e <path>   Path to env file (default: ~/.ddns or .ddns)\n"
                      << "  --loop <seconds>   Run continuously every N seconds\n"
                      << "  --help, -h         Show this help\n";
            return 0;
        }
    }

    if (!env_file_path.empty()) {
        load_env_file(env_file_path);
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            load_env_file(std::string(home) + "/.ddns");
        }
        load_env_file(".ddns");
    }

    if (interval == 0) {
        interval = std::atoi(get_env("CHECK_INTERVAL", "0").c_str());
    }

    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (interval > 0) {
        std::cout << "[INFO] Running Dynamic DNS updater every " << interval << " seconds..." << std::endl;
        while (true) {
            run_ddns_check();
            sleep(static_cast<unsigned int>(interval));
        }
    } else {
        run_ddns_check();
    }

    curl_global_cleanup();
    return 0;
}
