#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace app {

struct CpuMetrics {
    float overallPercent = 0.0F;
    std::vector<float> perCorePercent;
    float load1 = 0.0F;
    float load5 = 0.0F;
    float load15 = 0.0F;
    float frequencyMHz = 0.0F;
    uint64_t totalJiffies = 0;
};

struct MemoryMetrics {
    uint64_t totalMB = 0;
    uint64_t usedMB = 0;
    uint64_t availableMB = 0;
    uint64_t swapTotalMB = 0;
    uint64_t swapUsedMB = 0;
};

struct DiskEntry {
    std::string name;
    std::string mountPoint;
    double totalGB = 0.0;
    double usedGB = 0.0;
    double readKBps = 0.0;
    double writeKBps = 0.0;
};

struct NetworkEntry {
    std::string name;
    double downKBps = 0.0;
    double upKBps = 0.0;
    uint64_t totalRxBytes = 0;
    uint64_t totalTxBytes = 0;
};

struct GpuMetrics {
    bool available = false;
    float utilizationPercent = 0.0F;
    uint64_t vramUsedMB = 0;
    uint64_t vramTotalMB = 0;
    std::string note = "GPU metrics unavailable";
};

struct ProcessInfo {
    int pid = 0;
    std::string name;
    float cpuPercent = 0.0F;
    uint64_t memoryMB = 0;
};

struct Snapshot {
    uint64_t timestampMs = 0;
    std::string provider = "system";
    CpuMetrics cpu;
    MemoryMetrics memory;
    std::vector<DiskEntry> disks;
    std::vector<NetworkEntry> interfaces;
    GpuMetrics gpu;
    std::vector<ProcessInfo> topProcesses;
    std::string error;
};

} // namespace app
