#pragma once

#include "types.hpp"

#include <memory>

namespace app {

class IDataProvider {
  public:
    virtual ~IDataProvider() = default;
    virtual Snapshot collect() = 0;
};

std::unique_ptr<IDataProvider> make_system_provider();
std::unique_ptr<IDataProvider> make_mock_provider();

bool export_snapshot_json(const Snapshot& snapshot, const std::string& path);
bool export_snapshot_csv(const Snapshot& snapshot, const std::string& path);
bool kill_process(int pid, std::string& error);

} // namespace app
