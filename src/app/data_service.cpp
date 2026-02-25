#include "data_service.hpp"

#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <exception>

namespace app {

namespace {
std::shared_ptr<IDataProvider> to_shared(std::unique_ptr<IDataProvider> provider) {
    return std::shared_ptr<IDataProvider>(std::move(provider));
}
} // namespace

DataService::DataService(Settings settings)
    : settings_(settings), provider_(to_shared(settings.useMockData ? make_mock_provider() : make_system_provider())) {}

DataService::DataService(Settings settings, std::unique_ptr<IDataProvider> provider)
    : settings_(settings), provider_(to_shared(std::move(provider))) {
    if (!provider_) {
        provider_ = to_shared(settings.useMockData ? make_mock_provider() : make_system_provider());
    }
}

DataService::~DataService() { stop(); }

void DataService::start() {
    if (running_.exchange(true)) {
        return;
    }
    worker_ = std::thread(&DataService::run, this);
}

void DataService::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    wakeCv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void DataService::update_settings(const Settings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool providerChanged = settings_.useMockData != settings.useMockData;
    settings_ = settings;
    settings_.refreshMs = std::clamp(settings_.refreshMs, 100, 5000);

    if (providerChanged) {
        provider_ = to_shared(settings_.useMockData ? make_mock_provider() : make_system_provider());
        log_event("INFO", "service.provider", settings_.useMockData ? "switched to mock provider"
                                                                  : "switched to system provider");
    }
    wakeCv_.notify_all();
}

Snapshot DataService::latest_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

TimeSeries DataService::history() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

void DataService::push_value(std::deque<float>& buffer, float value) {
    constexpr size_t maxSamples = 60;
    buffer.push_back(value);
    while (buffer.size() > maxSamples) {
        buffer.pop_front();
    }
}

void DataService::run() {
    log_event("INFO", "service.start", "data collection started");

    while (running_.load()) {
        std::shared_ptr<IDataProvider> provider;
        int sleepMs = 1000;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            provider = provider_;
            sleepMs = std::clamp(settings_.refreshMs, 100, 5000);
        }

        Snapshot next;
        try {
            if (provider) {
                next = provider->collect();
            } else {
                next.error = "No active data provider";
            }
        } catch (const std::exception& ex) {
            next.error = ex.what();
            log_event("ERROR", "service.collect", ex.what());
        } catch (...) {
            next.error = "Unknown collection failure";
            log_event("ERROR", "service.collect", "unknown exception");
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_ = next;
            push_value(history_.cpu, next.cpu.overallPercent);
            const float memPct = next.memory.totalMB > 0 ? (100.0F * next.memory.usedMB / next.memory.totalMB) : 0.0F;
            push_value(history_.memory, memPct);

            double netKbps = 0;
            for (const auto& iface : next.interfaces) {
                netKbps += iface.downKBps + iface.upKBps;
            }
            push_value(history_.net, static_cast<float>(netKbps));

            double diskKbps = 0;
            for (const auto& disk : next.disks) {
                diskKbps += disk.readKBps + disk.writeKBps;
            }
            push_value(history_.disk, static_cast<float>(diskKbps));
        }

        std::unique_lock<std::mutex> lock(mutex_);
        wakeCv_.wait_for(lock, std::chrono::milliseconds(sleepMs), [this] { return !running_.load(); });
    }

    log_event("INFO", "service.stop", "data collection stopped");
}

} // namespace app
