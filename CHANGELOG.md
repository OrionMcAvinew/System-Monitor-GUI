# Changelog

## Unreleased

### Added
- Added conflict-resolution helper script `scripts/resolve_merge_conflicts.sh` for common PR conflict files (`CMakeLists.txt`, `README.md`, `CHANGELOG.md`).
- Added `.gitattributes` union merge strategy for key docs to reduce merge conflicts (`README.md`, `HOW_TO_USE.md`, `CHANGELOG.md`).
- Added dedicated usage walkthrough in `HOW_TO_USE.md`.
- DataService thread safety and shutdown responsiveness improvements (provider swap race fix, condition-variable wakeups, exception-safe collection).
- Added `test_data_service` to validate threaded polling/history behavior with injectable provider.
- Phase 2/3 completion pass with UI tables and polished page layout for Overview/Processes/Disks/Network/Settings.
- Process table improvements with safer confirmation dialog.
- Keyboard shortcut (`Ctrl+E`) for fast JSON snapshot export.
- Enhanced JSON/CSV export payloads including provider and disk aggregate fields.
- `.gitignore` for build artifacts and generated snapshots/logs.

### Changed
- Linux collector quality improvements:
  - network loopback filtering,
  - throughput now normalized by elapsed sample time,
  - process CPU now based on delta ticks across samples,
  - provider metadata included in snapshots.
- Settings parser hardened with clamped refresh interval and safe unit fallback.
- README refreshed with clearer architecture, usage, shortcuts, and troubleshooting.

### Fixed
- CMake now gracefully degrades when OpenGL is unavailable (skips GUI target with warning instead of hard configure failure unless `REQUIRE_OPENGL=ON`).
- Disk throughput units now calculated as KB/s over real elapsed time.
- Better initial loading behavior while first sample is being collected.
