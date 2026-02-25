#include "app/settings.hpp"

#include <cassert>
#include <fstream>

int main() {
    const std::string path = "test_settings.conf";
    app::Settings settings;
    settings.refreshMs = 750;
    settings.darkTheme = false;
    settings.debugMode = true;
    settings.useMockData = true;
    settings.units = "metric";

    app::save_settings(settings, path);
    app::Settings loaded = app::load_settings(path);

    assert(loaded.refreshMs == 750);
    assert(!loaded.darkTheme);
    assert(loaded.debugMode);
    assert(loaded.useMockData);
    assert(loaded.units == "metric");

    std::ofstream out(path, std::ios::trunc);
    out << "refresh_ms=1\n";
    out << "units=weird\n";
    out.close();

    loaded = app::load_settings(path);
    assert(loaded.refreshMs == 250);
    assert(loaded.units == "auto");
    return 0;
}
