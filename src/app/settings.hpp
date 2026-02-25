#pragma once

#include <string>

namespace app {

struct Settings {
    int refreshMs = 1000;
    bool darkTheme = true;
    bool debugMode = false;
    bool useMockData = false;
    std::string units = "auto";
};

Settings load_settings(const std::string& path);
void save_settings(const Settings& settings, const std::string& path);

} // namespace app
