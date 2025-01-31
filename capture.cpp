#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include "base64.h"  // Для кодирования payload
#include <regex>
#include <chrono>
#include <ctime>
#include <filesystem>

using json = nlohmann::json;

const std::string LOG_FILENAME = "traffic_log.json";
const size_t MAX_LOG_ENTRIES = 1000; // Максимальное число записей в логе
const size_t MAX_LOG_SIZE = 10 * 1024 * 1024; // 10MB

std::unordered_map<std::string, int> packetCount;
std::unordered_map<std::string, int> httpsRequestCount;
std::unordered_set<std::string> blockedDomains = {"malware.com", "phishing-site.net"};
std::unordered_set<std::string> blacklistedIPs = {"192.168.1.100", "10.0.0.5"};
std::vector<std::string> sqlInjectionPatterns = {"SELECT ", "DROP TABLE", "UNION SELECT", "INSERT INTO", "DELETE FROM"};
std::vector<std::string> xssPatterns = {"<script>", "alert(", "onerror="};
std::unordered_set<std::string> badUserAgents = {"python-requests", "curl", "nmap", "sqlmap"};

void rotateLogs() {
    struct stat fileStat;
    if (stat(LOG_FILENAME.c_str(), &fileStat) == 0 && fileStat.st_size >= MAX_LOG_SIZE) {
        std::time_t now = std::time(nullptr);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "logs/archive_%Y-%m-%d_%H-%M-%S.json", localtime(&now));
        std::filesystem::create_directories("logs");
        std::filesystem::rename(LOG_FILENAME, buffer);
    }
}

json loadExistingLogs() {
    std::ifstream log_file(LOG_FILENAME);
    if (!log_file.is_open()) return json::array();
    try {
        json logs;
        log_file >> logs;
        return logs.is_array() ? logs : json::array();
    } catch (...) {
        return json::array();
    }
}

void saveLogs(const json& logs) {
    json trimmedLogs = logs;
    if (logs.size() > MAX_LOG_ENTRIES) {
        trimmedLogs = json(logs.end() - MAX_LOG_ENTRIES, logs.end());
    }
    std::ofstream log_file(LOG_FILENAME, std::ios::trunc);
    log_file << trimmedLogs.dump(4);
}

bool detectSQLInjection(const std::string& payload) {
    for (const auto& pattern : sqlInjectionPatterns) {
        if (payload.find(pattern) != std::string::npos) return true;
    }
    return false;
}

bool detectXSS(const std::string& payload) {
    for (const auto& pattern : xssPatterns) {
        if (payload.find(pattern) != std::string::npos) return true;
    }
    return false;
}

bool detectDDoS(const std::string& src_ip) {
    httpsRequestCount[src_ip]++;
    return httpsRequestCount[src_ip] > 100;
}

bool checkHTTPSDomain(const std::string& payload) {
    std::regex sniPattern(".*?server_name (\\S+)");
    std::smatch match;
    if (std::regex_search(payload, match, sniPattern)) {
        std::string domain = match[1].str();
        return blockedDomains.count(domain);
    }
    return false;
}

bool analyzeHTTPPayload(const std::string& payload) {
    if (detectSQLInjection(payload) || detectXSS(payload)) return true;

    std::regex userAgentPattern("User-Agent: (.*?)\\r\\n");
    std::smatch match;
    if (std::regex_search(payload, match, userAgentPattern)) {
        std::string userAgent = match[1].str();
        return badUserAgents.count(userAgent);
    }
    return false;
}

void packetHandler(u_char* userData, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
    struct ip* ipHeader = (struct ip*)(packet + 14);
    
    char src_ip[INET_ADDRSTRLEN];
    char dest_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &(ipHeader->ip_src), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ipHeader->ip_dst), dest_ip, INET_ADDRSTRLEN);

    int protocol = ipHeader->ip_p;
    size_t payload_length = std::min<size_t>(pkthdr->len - 14 - ipHeader->ip_hl * 4, 1024);
    std::string payload(reinterpret_cast<const char*>(packet + 14 + ipHeader->ip_hl * 4), payload_length);
    std::string encoded_payload = base64_encode(reinterpret_cast<const unsigned char*>(payload.data()), payload.size());

    bool maliciousHTTP = (protocol == 6) && analyzeHTTPPayload(payload);
    bool maliciousSNI = (protocol == 443) && checkHTTPSDomain(payload);
    bool maliciousDDoS = (protocol == 443) && detectDDoS(src_ip);
    
    bool malicious = maliciousHTTP || maliciousSNI || maliciousDDoS;

    json log_entry = {
        {"timestamp", std::time(nullptr)},
        {"source_ip", src_ip},
        {"destination_ip", dest_ip},
        {"protocol", protocol},
        {"payload", encoded_payload},
        {"malicious", malicious},
        {"threat_type", maliciousSNI ? "Blocked HTTPS Domain" : (maliciousHTTP ? "HTTP Attack" : (maliciousDDoS ? "DDoS Attack" : "Other"))}
    };

    rotateLogs();
    json logs = loadExistingLogs();
    logs.push_back(log_entry);
    saveLogs(logs);
}

void startPacketCapture() {
    char errorBuffer[PCAP_ERRBUF_SIZE];
    pcap_if_t* alldevs;
    if (pcap_findalldevs(&alldevs, errorBuffer) == -1) {
        std::cerr << "Error finding devices: " << errorBuffer << std::endl;
        return;
    }
    pcap_if_t* device = alldevs;
    if (!device) {
        std::cerr << "No network interfaces found!" << std::endl;
        return;
    }
    std::cout << "Using device: " << device->name << std::endl;

    pcap_t* handle = pcap_open_live(device->name, BUFSIZ, 1, 1000, errorBuffer);
    if (!handle) {
        std::cerr << "Error opening device: " << errorBuffer << std::endl;
        return;
    }

    std::cout << "Starting packet capture. Press Ctrl+C to stop." << std::endl;
    pcap_loop(handle, 0, packetHandler, nullptr);

    pcap_close(handle);
    pcap_freealldevs(alldevs);
}
