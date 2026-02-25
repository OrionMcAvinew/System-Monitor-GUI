#include "app/data_service.hpp"

#include <cassert>
#include <chrono>
#include <thread>

namespace {
class FakeProvider : public app::IDataProvider {
  public:
    app::Snapshot collect() override {
        app::Snapshot s;
        s.timestampMs = ++tick;
        s.provider = "fake";
        s.cpu.overallPercent = static_cast<float>(tick % 100);
        s.memory.totalMB = 1000;
        s.memory.usedMB = 500;
        return s;
    }

  private:
    uint64_t tick = 0;
};
} // namespace

int main() {
    app::Settings settings;
    settings.refreshMs = 100;

    app::DataService service(settings, std::make_unique<FakeProvider>());
    service.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    service.stop();

    const auto snapshot = service.latest_snapshot();
    const auto history = service.history();

    assert(snapshot.timestampMs > 0);
    assert(snapshot.provider == "fake");
    assert(!history.cpu.empty());
    assert(history.cpu.size() <= 60);
    return 0;
}
