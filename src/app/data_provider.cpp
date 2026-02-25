#include "data_provider.hpp"

#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#ifdef __linux__
#include <csignal>
#include <dirent.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace app {
namespace {
uint64_t now_ms() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

class MockDataProvider : public IDataProvider {
  public:
    Snapshot collect() override {
        Snapshot s;
        s.timestampMs = now_ms();
        s.provider = "mock";
        tick_ += 0.12F;

        s.cpu.overallPercent = 50.0F + 35.0F * std::sin(tick_);
        s.cpu.perCorePercent = {s.cpu.overallPercent - 10.0F, s.cpu.overallPercent + 4.0F, s.cpu.overallPercent - 5.0F,
                                s.cpu.overallPercent + 8.0F};
        s.cpu.load1 = s.cpu.overallPercent / 100.0F * 4.0F;
        s.cpu.load5 = s.cpu.load1 * 0.9F;
        s.cpu.load15 = s.cpu.load1 * 0.7F;
        s.cpu.frequencyMHz = 3200.0F;

        s.memory.totalMB = 16384;
        s.memory.usedMB = static_cast<uint64_t>(8000 + 2000 * (1.0F + std::sin(tick_ * 0.7F)));
        s.memory.availableMB = s.memory.totalMB - s.memory.usedMB;
        s.memory.swapTotalMB = 4096;
        s.memory.swapUsedMB = static_cast<uint64_t>(500 + 200 * (1.0F + std::cos(tick_)));

        s.interfaces.push_back(
            {"eth0", 350 + 250 * std::abs(std::sin(tick_)), 80 + 30 * std::abs(std::cos(tick_)), 0, 0});
        s.interfaces.push_back({"wlan0", 20 + 10 * std::abs(std::cos(tick_ * 0.3F)),
                                10 + 5 * std::abs(std::sin(tick_ * 0.8F)), 0, 0});

        s.disks.push_back(
            {"sda", "/", 512.0, 380.0, 1024 * std::abs(std::sin(tick_)), 400 * std::abs(std::cos(tick_))});

        s.gpu.available = false;
        s.gpu.note = "Mock mode enabled";

        for (int i = 0; i < 12; ++i) {
            s.topProcesses.push_back({1000 + i, "process_" + std::to_string(i), 15.0F / static_cast<float>(i + 1),
                                      static_cast<uint64_t>(100 + i * 40)});
        }
        return s;
    }

  private:
    float tick_ = 0.0F;
};

#ifdef __linux__
struct CpuTimes {
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;
};

class LinuxSystemProvider : public IDataProvider {
  public:
    Snapshot collect() override {
        Snapshot s;
        s.timestampMs = now_ms();
        s.provider = "linux-system";

        read_cpu(s);
        read_memory(s);
        read_network(s);
        read_disks(s);
        read_processes(s);

        s.gpu.available = false;
        s.gpu.note = "GPU metrics unavailable on this build";
        if (s.cpu.perCorePercent.empty()) {
            s.error = "No CPU metrics available";
        }
        return s;
    }

  private:
    std::vector<CpuTimes> prevCpu_;
    uint64_t lastSampleMs_ = 0;
    std::map<std::string, std::pair<uint64_t, uint64_t>> prevNetBytes_;
    std::map<std::string, std::pair<uint64_t, uint64_t>> prevDiskSectors_;
    std::map<int, uint64_t> prevProcTicks_;
    uint64_t prevTotalCpuJiffies_ = 0;

    static float usage(const CpuTimes& prev, const CpuTimes& curr) {
        const uint64_t prevIdle = prev.idle + prev.iowait;
        const uint64_t idle = curr.idle + curr.iowait;
        const uint64_t prevNonIdle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
        const uint64_t nonIdle = curr.user + curr.nice + curr.system + curr.irq + curr.softirq + curr.steal;
        const uint64_t totald = (idle + nonIdle) - (prevIdle + prevNonIdle);
        const uint64_t idled = idle - prevIdle;
        if (totald == 0) {
            return 0.0F;
        }
        return 100.0F * static_cast<float>(totald - idled) / static_cast<float>(totald);
    }

    static uint64_t sum_jiffies(const CpuTimes& t) {
        return t.user + t.nice + t.system + t.idle + t.iowait + t.irq + t.softirq + t.steal;
    }

    void read_cpu(Snapshot& s) {
        std::ifstream in("/proc/stat");
        if (!in.is_open()) {
            log_event("WARN", "collector.cpu", "could not open /proc/stat");
            return;
        }

        std::string line;
        std::vector<CpuTimes> now;
        while (std::getline(in, line)) {
            if (line.rfind("cpu", 0) != 0) {
                break;
            }
            std::istringstream iss(line);
            std::string label;
            CpuTimes t;
            iss >> label >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
            now.push_back(t);
        }

        if (now.empty()) {
            return;
        }

        s.cpu.totalJiffies = sum_jiffies(now[0]);
        if (!prevCpu_.empty() && prevCpu_.size() == now.size()) {
            s.cpu.overallPercent = usage(prevCpu_[0], now[0]);
            for (size_t i = 1; i < now.size(); ++i) {
                s.cpu.perCorePercent.push_back(usage(prevCpu_[i], now[i]));
            }
        } else {
            s.cpu.perCorePercent.assign(now.size() > 1 ? now.size() - 1 : 1, 0.0F);
        }
        prevCpu_ = now;

        std::ifstream loadIn("/proc/loadavg");
        if (loadIn.is_open()) {
            loadIn >> s.cpu.load1 >> s.cpu.load5 >> s.cpu.load15;
        }

        std::ifstream cpuInfo("/proc/cpuinfo");
        while (std::getline(cpuInfo, line)) {
            if (line.find("cpu MHz") != std::string::npos) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    s.cpu.frequencyMHz = std::stof(line.substr(colon + 1));
                    break;
                }
            }
        }
    }

    void read_memory(Snapshot& s) {
        std::ifstream in("/proc/meminfo");
        if (!in.is_open()) {
            log_event("WARN", "collector.mem", "could not open /proc/meminfo");
            return;
        }

        std::map<std::string, uint64_t> mem;
        std::string key;
        uint64_t value = 0;
        std::string unit;
        while (in >> key >> value >> unit) {
            if (!key.empty() && key.back() == ':') {
                key.pop_back();
            }
            mem[key] = value;
        }

        const uint64_t totalKB = mem["MemTotal"];
        const uint64_t availKB = mem["MemAvailable"];
        s.memory.totalMB = totalKB / 1024;
        s.memory.availableMB = availKB / 1024;
        s.memory.usedMB = totalKB > availKB ? (totalKB - availKB) / 1024 : 0;
        s.memory.swapTotalMB = mem["SwapTotal"] / 1024;
        const uint64_t swapFree = mem["SwapFree"];
        s.memory.swapUsedMB = s.memory.swapTotalMB > (swapFree / 1024) ? s.memory.swapTotalMB - (swapFree / 1024) : 0;
    }

    void read_network(Snapshot& s) {
        std::ifstream in("/proc/net/dev");
        if (!in.is_open()) {
            log_event("WARN", "collector.net", "could not open /proc/net/dev");
            return;
        }

        std::string line;
        std::getline(in, line);
        std::getline(in, line);

        const uint64_t nowMs = now_ms();
        const double seconds = (lastSampleMs_ == 0) ? 1.0 : std::max(0.001, (nowMs - lastSampleMs_) / 1000.0);

        while (std::getline(in, line)) {
            auto colon = line.find(':');
            if (colon == std::string::npos) {
                continue;
            }

            std::string name = line.substr(0, colon);
            name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());
            if (name == "lo") {
                continue;
            }

            std::istringstream iss(line.substr(colon + 1));
            uint64_t rxBytes = 0;
            uint64_t txBytes = 0;
            uint64_t temp = 0;
            iss >> rxBytes;
            for (int i = 0; i < 7; ++i) {
                iss >> temp;
            }
            iss >> txBytes;

            auto& prev = prevNetBytes_[name];
            double down = 0.0;
            double up = 0.0;
            if (prev.first != 0 || prev.second != 0) {
                down = (rxBytes - prev.first) / 1024.0 / seconds;
                up = (txBytes - prev.second) / 1024.0 / seconds;
            }
            prev = {rxBytes, txBytes};
            s.interfaces.push_back({name, down, up, rxBytes, txBytes});
        }
        lastSampleMs_ = nowMs;
    }

    void read_disks(Snapshot& s) {
        std::ifstream mtab("/proc/mounts");
        if (!mtab.is_open()) {
            log_event("WARN", "collector.disk", "could not open /proc/mounts");
            return;
        }

        std::vector<std::pair<std::string, std::string>> mounts;
        std::string device;
        std::string mountPoint;
        std::string fsType;
        std::string opts;
        while (mtab >> device >> mountPoint >> fsType >> opts) {
            if (device.rfind("/dev/", 0) == 0) {
                mounts.emplace_back(device.substr(5), mountPoint);
            }
            std::getline(mtab, opts);
        }

        std::map<std::string, std::pair<uint64_t, uint64_t>> diskSectors;
        std::ifstream diskstats("/proc/diskstats");
        std::string line;
        while (std::getline(diskstats, line)) {
            std::istringstream iss(line);
            int major = 0;
            int minor = 0;
            std::string name;
            uint64_t readsDone = 0;
            uint64_t readsMerged = 0;
            uint64_t sectorsRead = 0;
            uint64_t msRead = 0;
            uint64_t writesDone = 0;
            uint64_t writesMerged = 0;
            uint64_t sectorsWritten = 0;
            uint64_t msWrite = 0;
            if (!(iss >> major >> minor >> name >> readsDone >> readsMerged >> sectorsRead >> msRead >> writesDone >>
                  writesMerged >> sectorsWritten >> msWrite)) {
                continue;
            }
            diskSectors[name] = {sectorsRead, sectorsWritten};
        }

        const double seconds = (lastSampleMs_ == 0) ? 1.0 : std::max(0.001, (now_ms() - lastSampleMs_) / 1000.0);
        for (const auto& [dev, mount] : mounts) {
            struct statvfs fs {};
            if (statvfs(mount.c_str(), &fs) != 0) {
                continue;
            }

            const double total = static_cast<double>(fs.f_blocks) * fs.f_frsize / (1024.0 * 1024.0 * 1024.0);
            const double avail = static_cast<double>(fs.f_bavail) * fs.f_frsize / (1024.0 * 1024.0 * 1024.0);
            const double used = total - avail;

            auto curr = diskSectors[dev];
            auto prev = prevDiskSectors_[dev];
            double readKBps = 0.0;
            double writeKBps = 0.0;
            if (prev.first != 0 || prev.second != 0) {
                readKBps = (curr.first - prev.first) * 512.0 / 1024.0 / seconds;
                writeKBps = (curr.second - prev.second) * 512.0 / 1024.0 / seconds;
            }
            prevDiskSectors_[dev] = curr;
            s.disks.push_back({dev, mount, total, used, readKBps, writeKBps});
        }
    }

    void read_processes(Snapshot& s) {
        DIR* dir = opendir("/proc");
        if (!dir) {
            log_event("WARN", "collector.proc", "could not open /proc");
            return;
        }

        std::vector<ProcessInfo> processes;
        std::set<int> activePids;
        struct dirent* entry;

        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) {
                continue;
            }
            int pid = std::atoi(entry->d_name);
            if (pid <= 0) {
                continue;
            }
            activePids.insert(pid);

            std::string statPath = std::string("/proc/") + entry->d_name + "/stat";
            std::ifstream statIn(statPath);
            if (!statIn.is_open()) {
                continue;
            }

            std::string statLine;
            std::getline(statIn, statLine);
            auto l = statLine.find('(');
            auto r = statLine.rfind(')');
            if (l == std::string::npos || r == std::string::npos || r <= l) {
                continue;
            }

            std::string name = statLine.substr(l + 1, r - l - 1);
            std::istringstream tail(statLine.substr(r + 2));
            std::vector<std::string> tokens;
            std::string tok;
            while (tail >> tok) {
                tokens.push_back(tok);
            }
            if (tokens.size() < 22) {
                continue;
            }

            const uint64_t procTicks = std::stoull(tokens[11]) + std::stoull(tokens[12]);
            const uint64_t procPrev = prevProcTicks_[pid];
            float cpuPct = 0.0F;
            if (procPrev > 0 && prevTotalCpuJiffies_ > 0 && s.cpu.totalJiffies > prevTotalCpuJiffies_) {
                const uint64_t procDelta = procTicks - procPrev;
                const uint64_t totalDelta = s.cpu.totalJiffies - prevTotalCpuJiffies_;
                cpuPct = 100.0F * static_cast<float>(procDelta) / static_cast<float>(totalDelta);
            }
            prevProcTicks_[pid] = procTicks;

            std::string statusPath = std::string("/proc/") + entry->d_name + "/status";
            std::ifstream statusIn(statusPath);
            uint64_t memKB = 0;
            std::string line;
            while (std::getline(statusIn, line)) {
                if (line.rfind("VmRSS:", 0) == 0) {
                    std::istringstream vm(line.substr(6));
                    vm >> memKB;
                    break;
                }
            }
            processes.push_back({pid, name, cpuPct, memKB / 1024});
        }
        closedir(dir);

        for (auto it = prevProcTicks_.begin(); it != prevProcTicks_.end();) {
            if (!activePids.count(it->first)) {
                it = prevProcTicks_.erase(it);
            } else {
                ++it;
            }
        }
        prevTotalCpuJiffies_ = s.cpu.totalJiffies;

        std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
            if (std::abs(a.cpuPercent - b.cpuPercent) < 0.001F) {
                return a.memoryMB > b.memoryMB;
            }
            return a.cpuPercent > b.cpuPercent;
        });

        if (processes.size() > 150) {
            processes.resize(150);
        }
        s.topProcesses = std::move(processes);
    }
};
#endif

} // namespace

std::unique_ptr<IDataProvider> make_system_provider() {
#ifdef __linux__
    return std::make_unique<LinuxSystemProvider>();
#else
    return std::make_unique<MockDataProvider>();
#endif
}

std::unique_ptr<IDataProvider> make_mock_provider() { return std::make_unique<MockDataProvider>(); }

bool export_snapshot_json(const Snapshot& snapshot, const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"timestamp\": " << snapshot.timestampMs << ",\n";
    out << "  \"provider\": \"" << snapshot.provider << "\",\n";
    out << "  \"cpu\": {\"overall_percent\": " << snapshot.cpu.overallPercent << ",\"mhz\":"
        << snapshot.cpu.frequencyMHz << "},\n";
    out << "  \"memory\": {\"used_mb\": " << snapshot.memory.usedMB << ",\"total_mb\": " << snapshot.memory.totalMB
        << ",\"available_mb\":" << snapshot.memory.availableMB << "},\n";
    out << "  \"interfaces\": [\n";
    for (size_t i = 0; i < snapshot.interfaces.size(); ++i) {
        const auto& itf = snapshot.interfaces[i];
        out << "    {\"name\":\"" << itf.name << "\",\"down_kbps\":" << itf.downKBps << ",\"up_kbps\":"
            << itf.upKBps << "}" << (i + 1 < snapshot.interfaces.size() ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"disks\": [\n";
    for (size_t i = 0; i < snapshot.disks.size(); ++i) {
        const auto& disk = snapshot.disks[i];
        out << "    {\"name\":\"" << disk.name << "\",\"mount\":\"" << disk.mountPoint << "\",\"used_gb\":"
            << disk.usedGB << ",\"total_gb\":" << disk.totalGB << "}" << (i + 1 < snapshot.disks.size() ? "," : "")
            << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

bool export_snapshot_csv(const Snapshot& snapshot, const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << "timestamp_ms,provider,cpu_overall_percent,memory_used_mb,memory_total_mb,total_net_down_kbps,total_net_up_kbps,total_disk_kbps\n";
    double down = 0;
    double up = 0;
    for (const auto& iface : snapshot.interfaces) {
        down += iface.downKBps;
        up += iface.upKBps;
    }
    double disk = 0;
    for (const auto& d : snapshot.disks) {
        disk += d.readKBps + d.writeKBps;
    }
    out << snapshot.timestampMs << ',' << snapshot.provider << ',' << snapshot.cpu.overallPercent << ','
        << snapshot.memory.usedMB << ',' << snapshot.memory.totalMB << ',' << down << ',' << up << ',' << disk << '\n';
    return true;
}

bool kill_process(int pid, std::string& error) {
#ifdef __linux__
    if (::kill(pid, SIGTERM) != 0) {
        error = "Failed to kill process. Check permissions.";
        return false;
    }
    return true;
#else
    (void)pid;
    error = "Process termination is unavailable on this platform";
    return false;
#endif
}

} // namespace app
