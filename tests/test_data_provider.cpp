#include "app/data_provider.hpp"

#include <cassert>
#include <fstream>
#include <sstream>

int main() {
    auto provider = app::make_mock_provider();
    auto snapshot = provider->collect();

    assert(snapshot.timestampMs > 0);
    assert(snapshot.memory.totalMB > 0);
    assert(snapshot.cpu.perCorePercent.size() >= 1);
    assert(!snapshot.interfaces.empty());
    assert(snapshot.provider == "mock");

    assert(app::export_snapshot_json(snapshot, "test_snapshot.json"));
    assert(app::export_snapshot_csv(snapshot, "test_snapshot.csv"));

    std::ifstream json("test_snapshot.json");
    std::stringstream buf;
    buf << json.rdbuf();
    assert(buf.str().find("\"provider\": \"mock\"") != std::string::npos);
    return 0;
}
