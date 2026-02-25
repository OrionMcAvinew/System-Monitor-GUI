# System Monitor GUI

A cross-platform C++ desktop system monitor (Linux-first, graceful fallback on macOS/Windows) built with Dear ImGui + ImPlot + GLFW/OpenGL.

## Highlights

- Sidebar pages: **Overview**, **Processes**, **Disks**, **Network**, **Settings**.
- Background data polling (`DataService`) keeps rendering responsive and now supports fast shutdown/reconfiguration wakeups.
- Linux system collector with mock fallback provider.
- CPU: overall/per-core usage, frequency, load average.
- Memory: used/available + swap.
- Disks: per-disk capacity and read/write throughput.
- Network: per-interface throughput + total RX/TX counters.
- Processes: top list, search/filter, kill with confirmation.
- History graphs (60s): CPU, memory, network, disk.
- Snapshot export: JSON and CSV.
- Structured logging + debug mode.
- Persisted settings: refresh rate, theme, units, debug mode, mock mode.

## Architecture

- **UI layer**: `src/ui/dashboard.cpp` (page rendering, tables, dialogs, controls).
- **State/service layer**: `src/app/data_service.*` (threaded polling, latest snapshot, bounded history).
- **Data provider layer**: `src/app/data_provider.*` (`IDataProvider`, Linux collector, mock collector, export, process control).
- **App utilities**: `src/app/settings.*`, `src/app/logger.*`, typed models in `src/app/types.hpp`.

## Build

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/SystemMonitor
```

If OpenGL dev libraries are missing, configuration now falls back to core/test targets (GUI skipped) with a warning instead of failing hard.
Use `-DREQUIRE_OPENGL=ON` to enforce GUI requirements during configure.

## Test

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_GUI=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Includes `test_settings`, `test_data_provider`, and `test_data_service`.

## Usage

- **Overview**: quick health view + export actions.
- **Processes**: search by PID/name, terminate process with confirmation.
- **Disks/Network**: table views with per-resource throughput.
- **Settings**:
  - refresh interval,
  - dark/light theme,
  - debug mode,
  - units mode,
  - mock provider toggle.

### Keyboard shortcuts

- `Ctrl+E`: export JSON snapshot (`snapshot.json`).

## How-To Guide

For a full walkthrough of pages, actions, exports, settings, and troubleshooting workflow, see **[HOW_TO_USE.md](HOW_TO_USE.md)**.

## Troubleshooting

- **No real metrics**: enable mock provider in Settings.
- **Process kill fails**: permissions may be required.
- **No GUI build**: install OpenGL/GLFW dev dependencies.
- **GPU section unavailable**: currently best-effort placeholder.
- **Merge conflicts in docs**: this repo now uses `.gitattributes` `merge=union` for `README.md`, `HOW_TO_USE.md`, and `CHANGELOG.md` to reduce conflict friction; if duplicates appear, keep the newest wording.

## Screenshots (placeholders)

- `docs/screenshots/overview.png`
- `docs/screenshots/processes.png`
- `docs/screenshots/settings.png`

## Cross-platform notes

- Linux: full provider implemented.
- macOS/Windows: app runs with mock fallback until native collectors are added.
