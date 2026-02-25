#include "settings.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

namespace app {

Settings load_settings(const std::string& path) {
    Settings s;
    std::ifstream in(path);
    if (!in.is_open()) {
        return s;
    }

    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "refresh_ms") {
            const int parsed = static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
            s.refreshMs = std::clamp(parsed, 250, 5000);
        } else if (key == "dark_theme") {
            s.darkTheme = (value == "1");
        } else if (key == "debug_mode") {
            s.debugMode = (value == "1");
        } else if (key == "use_mock_data") {
            s.useMockData = (value == "1");
        } else if (key == "units") {
            s.units = (value == "metric") ? "metric" : "auto";
        }
    }
    return s;
}

void save_settings(const Settings& settings, const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << "refresh_ms=" << settings.refreshMs << '\n';
    out << "dark_theme=" << (settings.darkTheme ? "1" : "0") << '\n';
    out << "debug_mode=" << (settings.debugMode ? "1" : "0") << '\n';
    out << "use_mock_data=" << (settings.useMockData ? "1" : "0") << '\n';
    out << "units=" << settings.units << '\n';
}

} // namespace app
