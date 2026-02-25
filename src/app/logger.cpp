#include "logger.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>

namespace app {
namespace {
std::mutex g_logMutex;
bool g_debugEnabled = false;

uint64_t now_ms() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}
} // namespace

void set_debug_logging(bool enabled) { g_debugEnabled = enabled; }

void log_event(const std::string& level,
               const std::string& event,
               const std::string& message,
               const std::map<std::string, std::string>& fields) {
    if (level == "DEBUG" && !g_debugEnabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream out("system-monitor.log", std::ios::app);
    if (!out.is_open()) {
        return;
    }
    out << "{\"ts\":" << now_ms() << ",\"level\":\"" << level << "\",\"event\":\"" << event
        << "\",\"message\":\"" << message << "\"";
    for (const auto& [k, v] : fields) {
        out << ",\"" << k << "\":\"" << v << "\"";
    }
    out << "}\n";
    if (g_debugEnabled) {
        std::cerr << level << " " << event << " " << message << "\n";
    }
}

} // namespace app
