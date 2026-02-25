#include "dashboard.hpp"

#include "app/data_provider.hpp"
#include "app/data_service.hpp"
#include "app/logger.hpp"
#include "app/settings.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace ui {
namespace {
app::Settings g_settings;
std::unique_ptr<app::DataService> g_service;
std::string g_settingsPath = "settings.conf";
std::string g_status;
int g_page = 0;
char g_processFilter[128] = "";
int g_pidPendingKill = -1;

std::string rate_label(double kbps) {
    if (kbps > 1024.0) {
        return std::to_string(static_cast<int>(kbps / 1024.0)) + " MB/s";
    }
    return std::to_string(static_cast<int>(kbps)) + " KB/s";
}

void apply_theme() {
    if (g_settings.darkTheme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }
}

void plot_series(const char* title, const std::deque<float>& values, float yMax = 0.0F) {
    if (!ImPlot::BeginPlot(title, ImVec2(-1, 155))) {
        return;
    }
    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
    if (yMax > 0.0F) {
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, yMax, ImGuiCond_Always);
    }
    if (!values.empty()) {
        std::vector<float> contiguous(values.begin(), values.end());
        ImPlot::PlotLine(title, contiguous.data(), static_cast<int>(contiguous.size()));
    }
    ImPlot::EndPlot();
}

bool process_visible(const app::ProcessInfo& p) {
    std::string filter = g_processFilter;
    if (filter.empty()) {
        return true;
    }
    std::string name = p.name;
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name.find(filter) != std::string::npos || std::to_string(p.pid).find(filter) != std::string::npos;
}

} // namespace

void initialize_dashboard() {
    g_settings = app::load_settings(g_settingsPath);
    app::set_debug_logging(g_settings.debugMode);
    apply_theme();
    g_service = std::make_unique<app::DataService>(g_settings);
    g_service->start();
}

void shutdown_dashboard() {
    if (g_service) {
        g_service->stop();
        g_service.reset();
    }
    app::save_settings(g_settings, g_settingsPath);
}

void render_dashboard() {
    if (!g_service) {
        initialize_dashboard();
    }
    const auto snapshot = g_service->latest_snapshot();
    const auto history = g_service->history();

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E)) {
        g_status = app::export_snapshot_json(snapshot, "snapshot.json") ? "Saved snapshot.json" : "Export failed";
    }

    ImGui::Begin("System Monitor");
    ImGui::Columns(2, nullptr, true);
    ImGui::SetColumnWidth(0, 190.0F);

    const std::array<const char*, 5> pages = {"Overview", "Processes", "Disks", "Network", "Settings"};
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        if (ImGui::Selectable(pages[i], g_page == i)) {
            g_page = i;
        }
    }

    ImGui::NextColumn();
    ImGui::Text("Source: %s", snapshot.provider.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| Ctrl+E export JSON");

    if (!snapshot.error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.45F, 0.4F, 1), "Error: %s", snapshot.error.c_str());
    }

    if (snapshot.timestampMs == 0) {
        ImGui::TextDisabled("Collecting first sample...");
        ImGui::Columns(1);
        ImGui::End();
        return;
    }

    if (g_page == 0) {
        ImGui::Text("CPU %.1f%% | %.0f MHz | load %.2f %.2f %.2f", snapshot.cpu.overallPercent, snapshot.cpu.frequencyMHz,
                    snapshot.cpu.load1, snapshot.cpu.load5, snapshot.cpu.load15);
        if (!snapshot.cpu.perCorePercent.empty()) {
            ImGui::Text("Per-core usage:");
            for (size_t i = 0; i < snapshot.cpu.perCorePercent.size(); ++i) {
                ImGui::BulletText("Core %zu: %.1f%%", i, snapshot.cpu.perCorePercent[i]);
            }
        }

        const float memPct = snapshot.memory.totalMB > 0 ? (100.0F * snapshot.memory.usedMB / snapshot.memory.totalMB) : 0.0F;
        ImGui::Text("Memory %llu / %llu MB (%.1f%%)",
                    static_cast<unsigned long long>(snapshot.memory.usedMB),
                    static_cast<unsigned long long>(snapshot.memory.totalMB), memPct);
        ImGui::ProgressBar(memPct / 100.0F, ImVec2(-1, 0));
        ImGui::Text("Swap %llu / %llu MB",
                    static_cast<unsigned long long>(snapshot.memory.swapUsedMB),
                    static_cast<unsigned long long>(snapshot.memory.swapTotalMB));

        double netTotal = 0;
        for (const auto& iface : snapshot.interfaces) {
            netTotal += iface.downKBps + iface.upKBps;
        }
        ImGui::Text("Network total: %s", rate_label(netTotal).c_str());

        plot_series("CPU History (60s)", history.cpu, 100.0F);
        plot_series("Memory History (60s)", history.memory, 100.0F);
        plot_series("Network History (60s)", history.net);
        plot_series("Disk History (60s)", history.disk);

        if (ImGui::Button("Export JSON snapshot")) {
            g_status = app::export_snapshot_json(snapshot, "snapshot.json") ? "Saved snapshot.json" : "Export failed";
        }
        ImGui::SameLine();
        if (ImGui::Button("Export CSV snapshot")) {
            g_status = app::export_snapshot_csv(snapshot, "snapshot.csv") ? "Saved snapshot.csv" : "Export failed";
        }
    } else if (g_page == 1) {
        ImGui::InputTextWithHint("##filter", "Filter by process name or PID", g_processFilter, sizeof(g_processFilter));
        ImGui::Separator();

        if (ImGui::BeginTable("process_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("PID");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("CPU %");
            ImGui::TableSetupColumn("Memory MB");
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();

            for (const auto& process : snapshot.topProcesses) {
                if (!process_visible(process)) {
                    continue;
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", process.pid);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(process.name.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f", process.cpuPercent);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%llu", static_cast<unsigned long long>(process.memoryMB));
                ImGui::TableSetColumnIndex(4);
                std::string btn = "Kill##" + std::to_string(process.pid);
                if (ImGui::SmallButton(btn.c_str())) {
                    g_pidPendingKill = process.pid;
                    ImGui::OpenPopup("ConfirmKill");
                }
            }
            ImGui::EndTable();
        }

        if (ImGui::BeginPopupModal("ConfirmKill", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Terminate PID %d?", g_pidPendingKill);
            ImGui::Separator();
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                std::string error;
                g_status = app::kill_process(g_pidPendingKill, error) ? "Process terminated" : error;
                g_pidPendingKill = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                g_pidPendingKill = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    } else if (g_page == 2) {
        if (snapshot.disks.empty()) {
            ImGui::TextDisabled("No disk data available");
        }

        if (ImGui::BeginTable("disk_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Disk");
            ImGui::TableSetupColumn("Mount");
            ImGui::TableSetupColumn("Usage");
            ImGui::TableSetupColumn("Read");
            ImGui::TableSetupColumn("Write");
            ImGui::TableHeadersRow();

            for (const auto& disk : snapshot.disks) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(disk.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(disk.mountPoint.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f / %.1f GB", disk.usedGB, disk.totalGB);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(rate_label(disk.readKBps).c_str());
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(rate_label(disk.writeKBps).c_str());
            }
            ImGui::EndTable();
        }
    } else if (g_page == 3) {
        if (snapshot.interfaces.empty()) {
            ImGui::TextDisabled("No network interfaces available");
        }

        if (ImGui::BeginTable("net_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Interface");
            ImGui::TableSetupColumn("Down");
            ImGui::TableSetupColumn("Up");
            ImGui::TableSetupColumn("Total RX MB");
            ImGui::TableSetupColumn("Total TX MB");
            ImGui::TableHeadersRow();

            for (const auto& iface : snapshot.interfaces) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(iface.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(rate_label(iface.downKBps).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(rate_label(iface.upKBps).c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.1f", iface.totalRxBytes / 1024.0 / 1024.0);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.1f", iface.totalTxBytes / 1024.0 / 1024.0);
            }
            ImGui::EndTable();
        }
    } else if (g_page == 4) {
        int refresh = g_settings.refreshMs;
        if (ImGui::SliderInt("Refresh rate (ms)", &refresh, 250, 5000)) {
            g_settings.refreshMs = refresh;
            g_service->update_settings(g_settings);
        }

        if (ImGui::Checkbox("Dark theme", &g_settings.darkTheme)) {
            apply_theme();
        }
        if (ImGui::Checkbox("Debug mode", &g_settings.debugMode)) {
            app::set_debug_logging(g_settings.debugMode);
        }

        static int units = 0;
        units = (g_settings.units == "metric") ? 1 : 0;
        if (ImGui::Combo("Units", &units, "Auto\0Metric\0")) {
            g_settings.units = (units == 1) ? "metric" : "auto";
        }

        ImGui::Checkbox("Use mock data provider", &g_settings.useMockData);
        if (ImGui::Button("Apply provider change")) {
            g_service->update_settings(g_settings);
            g_status = "Provider updated";
        }
        if (ImGui::Button("Save settings")) {
            app::save_settings(g_settings, g_settingsPath);
            g_status = "Settings saved";
        }
        ImGui::TextWrapped("Accessibility: keyboard navigation works for sidebar/pages and process controls.");
        ImGui::TextWrapped("GPU support is best-effort and currently unavailable on this build.");
    }

    if (!g_status.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", g_status.c_str());
    }

    ImGui::Columns(1);
    ImGui::End();
}

} // namespace ui
