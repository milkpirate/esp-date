# Changelog

All notable changes to this project are documented in this file.

The format follows Keep a Changelog and the project adheres to Semantic Versioning.

## [Unreleased]
### Added
- `ESPDateConfig::usePSRAMBuffers` to prefer PSRAM-backed allocation for ESPDate-owned text/state buffers (timezone, NTP server, scoped TZ restore state) through `ESPBufferManager`, with automatic fallback to normal heap.
- Multi-server NTP config support in `ESPDateConfig` via `ntpServer` (primary), `ntpServer2` (secondary), and `ntpServer3` (tertiary), with empty values ignored and compacted before calling `configTzTime`.
- `isDstActive` helper to detect whether daylight saving time is in effect using a provided POSIX TZ string, the stored TZ config, or the current system TZ.
- Moon phase calculation helpers returning phase angle and illumination for any `DateTime` (or `now()`).
- Local time helpers: `nowLocal()` plus `toLocal(DateTime[, tz])` expose broken-out local components and UTC offset for debugging sunrise/sunset and DST handling.
- Documented the recommended UTC storage + local UI workflow (convert user-picked local times back to UTC with `fromLocal`/`parseDateTimeLocal`).
- `setNtpSyncCallback(...)` so applications can optionally react when SNTP reports a successful sync.
- `setNtpSyncCallback(const NtpSyncCallable&)` overload so member methods can be registered via `std::bind`.
- `syncNTP()` to immediately trigger a new SNTP sync using the configured NTP server list.
- `ntpSyncIntervalMs` config field plus `setNtpSyncIntervalMs(...)` runtime setter to override SNTP sync interval when runtime support is available.
- Internal last-sync tracking plus `hasLastNtpSync()` / `lastNtpSync()` getters.
- String formatting helpers for `DateTime`/`LocalDateTime` with buffer-based APIs plus `std::string` convenience wrappers (`nowUtcString`, `nowLocalString`, etc.).
- Added `lastNtpSyncStringLocal/Utc` helpers (buffer + `std::string`) to serialize the last NTP sync timestamp directly.
- Added direct `DateTime::utcString/localString` and `LocalDateTime::localString` methods so standalone values can be formatted without an `ESPDate` round-trip.
- Added focused example sketches: `examples/string_helpers` and `examples/ntp_sync_tracking`.

### Changed
- Replaced the `ESPDateConfig` constructor with an explicit `init(const ESPDateConfig&)` so configuration happens after the Arduino runtime is alive, avoiding early SNTP watchdog resets on some boards.
- `ESPDateConfig` now accepts up to three NTP servers; when at least one is provided alongside `timeZone`, `init` calls `configTzTime` to set the TZ and bootstrap SNTP automatically.

### Fixed
- Restored builds by adding the missing internal `utils.h` helpers referenced by the sun/scheduler code paths.
- Resolved ambiguous `setNtpSyncCallback(...)` overload selection for non-capturing lambdas on ESP32 toolchains.
- Added `ESPDate::deinit()` and destructor cleanup so a destroyed active instance releases SNTP callback ownership instead of leaving stale global callback state.

## [1.0.1] - 2025-12-09
### Added
- Sunrise/sunset helpers with optional constructor config (lat/lon/TZ), DST-aware TZ-string support, explicit parameter overloads, and `isDay` convenience checks (with optional offsets).
- Unity smoke tests under `test/test_esp_date` to cover arithmetic, formatting, parsing, and sun-cycle flows on-device.
- `monthName(int|DateTime)` helper to return friendly month strings.
- Sun cycle example sketch under `examples/sun_cycle`.
- Minute-level equality helpers: `isEqualMinutes` / `isEqualMinutesUtc`.
- Documented both installation paths (manual zip drop-in and PlatformIO `lib_deps` GitHub URL) in the README Getting Started section.
- Convenience constructors (`fromUtc`, `fromLocal`), start-of-year helpers, and scheduler-friendly helpers (`nextDailyAtLocal`, `nextWeekdayAtLocal`) to simplify consumer code.

### Fixed
- Seeded the CI Arduino CLI setup with the ESP32 board manager URL so core installs succeed on clean runners.
- Clarified date arithmetic/difference semantics in README and downgraded the `time_t` width check to a warning for 32-bit toolchains.

## [1.0.0] - 2025-12-07
### Added
- Introduced the `DateTime` value type backed by epoch seconds with UTC calendar accessors.
- Added arithmetic helpers for seconds, minutes, hours, days, months, and years, plus calendar boundaries (`startOfDay*`, `endOfMonth*`) in UTC and local time.
- Implemented formatting helpers (ISO-8601, datetime, date, time, and custom patterns) and parsing for ISO-8601 UTC or local `YYYY-MM-DD HH:MM:SS`.
- Included comparison/difference helpers (`isBefore`, `isAfter`, `isEqual`, `isSameDay`, `differenceIn*`) and convenience setters like `setTimeOfDayLocal`.
- Added a basic Arduino sketch demonstrating common flows.

### Tooling
- Added Arduino and PlatformIO metadata, CMake scaffolding, CI/release workflows, and repo hygiene files (code of conduct, license, changelog).

### Documentation
- Authored the README describing goals, feature overview, examples, gotchas, and ESPToolKit links.

[Unreleased]: https://github.com/ESPToolKit/esp-date/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/ESPToolKit/esp-date/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/ESPToolKit/esp-date/releases/tag/v1.0.0
