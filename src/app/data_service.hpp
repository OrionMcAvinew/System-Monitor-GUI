#pragma once

#include "data_provider.hpp"
#include "settings.hpp"
#include "types.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace app {

struct TimeSeries {
    std::deque<float> cpu;
    std::deque<float> memory;
    std::deque<float> net;
    std::deque<float> disk;
};

class DataService {
  public:
    explicit DataService(Settings settings);
    DataService(Settings settings, std::unique_ptr<IDataProvider> provider);
    ~DataService();

    void start();
    void stop();
    void update_settings(const Settings& settings);

    Snapshot latest_snapshot() const;
    TimeSeries history() const;

  private:
    void run();
    void push_value(std::deque<float>& buffer, float value);

    mutable std::mutex mutex_;
    std::condition_variable wakeCv_;
    Settings settings_;
    std::shared_ptr<IDataProvider> provider_;
    Snapshot snapshot_;
    TimeSeries history_;

    std::atomic<bool> running_ = false;
    std::thread worker_;
};

} // namespace app
