#include "httplib.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <thread>
#include <shared_mutex>

using json = nlohmann::json;

std::shared_mutex log_mutex;
std::vector<json> logs;

void loadLogs() {
    std::ifstream logFile("traffic_log.json");
    if (!logFile) return;

    json temp;
    logFile >> temp;

    std::unique_lock<std::shared_mutex> lock(log_mutex); // Блокируем перед обновлением logs

    if (temp.is_array()) {
        logs = temp.get<std::vector<json>>();
    }  
}

std::string getLogs(size_t limit = 1000) {
    std::shared_lock<std::shared_mutex> lock(log_mutex);
    json result = json::array();  // JSON массив

    size_t start = logs.size() > limit ? logs.size() - limit : 0;
    for (size_t i = start; i < logs.size(); ++i) {
        result.push_back(logs[i]);
    }

    return result.dump();
}

void runServer(){
    httplib::Server svr;

    // API для получения логов
    svr.Get("/logs", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(getLogs(), "application/json");
    });

    // Загружаем логи в отдельном потоке
    std::thread logLoader(loadLogs);
    logLoader.detach();

    std::cout << "Server started at http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);


}