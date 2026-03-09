# ESPDate

ESPDate is a tiny C++17 helper for ESP32 projects that makes working with dates and times feel more like using `date-fns` in JavaScript/TypeScript. It wraps `time_t` / `struct tm` and adds safe arithmetic, comparisons, and formatting in a single class-based API.

## CI / Release / License
[![CI](https://github.com/ESPToolKit/esp-date/actions/workflows/ci.yml/badge.svg)](https://github.com/ESPToolKit/esp-date/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/ESPToolKit/esp-date?sort=semver)](https://github.com/ESPToolKit/esp-date/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.md)

## Features
- **DateTime wrapper**: one small `DateTime` value type instead of juggling raw `time_t` + `struct tm`.
- **Safe arithmetic helpers**: `add/subSeconds`, `add/subMinutes`, `add/subHours`, `add/subDays`, `add/subMonths`, `add/subYears`.
- **Differences & comparisons**: `differenceIn*`, `isBefore`, `isAfter`, `isEqual`, `isSameDay`.
- **Minute-level comparisons**: `isEqualMinutes`/`isEqualMinutesUtc` for coarse equality.
- **Calendar helpers**: `startOfDay*`, `endOfDay*`, `startOfMonth*`, `endOfMonth*`, `isLeapYear`, `daysInMonth`, getters for year/month/day/weekday.
- **Formatting / parsing**: ISO-8601 and `YYYY-MM-DD HH:MM:SS` helpers, plus `strftime`-style patterns for UTC or local time.
- **String helpers**: embedded-safe buffer methods and `std::string` convenience wrappers for `DateTime`, `LocalDateTime`, `nowUtc`, and `nowLocal`.
- **Direct value formatting**: `DateTime::localString/utcString` and `LocalDateTime::localString` let individual values format themselves.
- **Sunrise / sunset**: compute daily sun times from lat/lon using numeric offsets or POSIX TZ strings (auto-DST aware).
- **DST detection**: `isDstActive` reports whether daylight saving time applies using the stored TZ, an explicit POSIX TZ string, or the current system TZ.
- **Moon phase**: `moonPhase` returns the current lunar phase angle and illumination fraction for any moment.
- **Optional NTP bootstrap**: call `init` with `ESPDateConfig` containing `timeZone` and at least one NTP server (`ntpServer`, optional `ntpServer2`/`ntpServer3`) to set TZ and start SNTP after Arduino/WiFi is ready.
- **NTP sync callback + manual re-sync**: register `setNtpSyncCallback(...)` with a function, lambda, or `std::bind`, call `syncNTP()` anytime to trigger an immediate refresh, and optionally override SNTP interval via `ntpSyncIntervalMs` / `setNtpSyncIntervalMs(...)`.
- **Optional PSRAM-backed config/state buffers**: `ESPDateConfig::usePSRAMBuffers` routes ESPDate-owned text state (timezone/NTP/scoped TZ restore buffers) through `ESPBufferManager` with automatic fallback.
- **Explicit lifecycle cleanup**: `deinit()` unregisters ESPDate-owned SNTP callback hooks, clears runtime config buffers, and is safe to call repeatedly; the destructor calls it automatically.
- **Init-state introspection**: `isInitialized()` reports whether `init(...)` has been called without a matching `deinit()`.
- **Last sync tracking**: `hasLastNtpSync()` / `lastNtpSync()` expose the latest SNTP sync timestamp kept inside `ESPDate`.
- **Last sync string helpers**: `lastNtpSyncStringLocal/Utc` provide direct formatting helpers for `lastNtpSync`.
- **Local breakdown helpers**: `nowLocal()` / `toLocal()` surface the broken-out local time (with UTC offset) for quick DST/debug checks; feed sunrise/sunset results into `toLocal` to read them in local time.
- **Friendly month names**: `monthName(int|DateTime)` returns `"January"` … `"December"` for quick labels.
- **Class-based API**: everything hangs off a single `ESPDate` instance; no global namespace clutter.
- **Lightweight & portable**: C++17, header-first public API; relies only on standard C time functions and the system clock (`time()`).

ESPDate does not configure SNTP by default. Call `init` with a POSIX TZ string plus at least one NTP server (`ntpServer`, optional `ntpServer2`/`ntpServer3`) to have ESPDate call `configTzTime` for you. Do this after the Arduino runtime and WiFi are up to avoid early watchdog resets. Otherwise you remain in control of time-zone setup and system clock sync.
`syncNTP()` returns `true` only when one or more NTP servers are configured and the runtime supports `configTzTime`.
SNTP exposes a system-level sync hook, so the last `setNtpSyncCallback(...)` registration is the active callback.
For the same reason, `lastNtpSync()` is tracked on the currently active `ESPDate` instance.
Example member-method binding style:
`date.setNtpSyncCallback(std::bind(&App::handleNTPSync, this, std::placeholders::_1));`
Set interval from config or at runtime:
`date.setNtpSyncIntervalMs(15 * 60 * 1000); // 15 minutes`

## Getting Started
Install one of two ways:
- Download the repository zip from GitHub, extract it, and drop the folder into your PlatformIO `lib/` directory, Arduino IDE `libraries/` directory, or add it as an ESP-IDF component.
- Add the public GitHub URL to `lib_deps` in `platformio.ini` so PlatformIO fetches it for you:
  ```
  lib_deps = https://github.com/ESPToolKit/esp-date.git
  ```

Then include the umbrella header:

```cpp
#include <Arduino.h>
#include <ESPDate.h>

// Create instances globally; configure them in setup once Arduino/WiFi are ready
ESPDate date;
ESPDate solar;

void setup() {
    Serial.begin(115200);

    // Configure TZ + NTP after WiFi is connected if you want ESPDate to call configTzTime
    ESPDateConfig dateCfg{0.0f, 0.0f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", 15 * 60 * 1000};
    dateCfg.ntpServer2 = "time.google.com";
    dateCfg.ntpServer3 = "time.cloudflare.com";
    dateCfg.usePSRAMBuffers = true; // optional: best effort, falls back automatically on non-PSRAM boards
    date.init(dateCfg);

    // Single-server config remains valid as before.
    ESPDateConfig solarCfg{47.4979f, 19.0402f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", 15 * 60 * 1000};
    solarCfg.usePSRAMBuffers = true;
    solar.init(solarCfg);

    date.setNtpSyncCallback([](const DateTime& syncedAtUtc) {
        Serial.printf("NTP synced at %lld\n", static_cast<long long>(syncedAtUtc.epochSeconds));
    });
    date.setNtpSyncIntervalMs(10 * 60 * 1000); // optional runtime update: 10 minutes
    date.syncNTP(); // optional: force an immediate re-sync
    if (date.hasLastNtpSync()) {
        Serial.printf("Last NTP sync: %lld\n", static_cast<long long>(date.lastNtpSync().epochSeconds));
        char lastSyncBuf[32];
        if (date.lastNtpSyncStringLocal(lastSyncBuf, sizeof(lastSyncBuf))) {
            Serial.printf("Last NTP sync local: %s\n", lastSyncBuf);
        }
    }

    // Make sure system time is set up (SNTP / manual) before calling date.now()

    DateTime now = date.now();             // current time from system clock
    DateTime lastYear = date.subYears(1);  // 1 year before now

    int64_t diffSeconds = date.differenceInSeconds(now, lastYear);
    int64_t diffDays    = date.differenceInDays(now, lastYear);

    bool isBefore = date.isBefore(lastYear, now); // true

    char buf[32];
    if (date.formatUtc(now, ESPDateFormat::Iso8601, buf, sizeof(buf))) {
        Serial.print("Now (UTC): ");
        Serial.println(buf);
    }

    Serial.print("Seconds between now and last year: ");
    Serial.println(diffSeconds);
    Serial.print("Days between now and last year: ");
    Serial.println(diffDays);

    LocalDateTime local = date.nowLocal();  // quick DST/local sanity check
    if (local.ok) {
        Serial.printf("Local now: %04d-%02d-%02d %02d:%02d:%02d (UTC offset %+d min)\n",
            local.year, local.month, local.day,
            local.hour, local.minute, local.second,
            local.offsetMinutes
        );
    }

    char localBuf[32];
    if (date.nowLocalString(localBuf, sizeof(localBuf))) {
        Serial.printf("Local now (string): %s\n", localBuf);
    }
    std::string utcString = date.nowUtcString();
    Serial.printf("UTC now (string): %s\n", utcString.c_str());
}

void loop() {
    // Example teardown path (mode switch / OTA / feature shutdown).
    static bool released = false;
    if (!released && millis() > 60000UL) {
        if (date.isInitialized()) {
            date.deinit();
        }
        if (solar.isInitialized()) {
            solar.deinit();
        }
        released = true;
    }
}
```

### Working With Local Time (UI) vs UTC (storage/logic)
- Show users **local** values: format with `formatLocal` or break down with `toLocal/nowLocal`.
- Store and compare **UTC**: keep `DateTime` as UTC epoch seconds so comparisons are consistent.
- Converting user choices back to UTC:

```cpp
// User picked "2025-03-05 21:30" in local time (UI)
DateTime when = date.fromLocal(2025, 3, 5, 21, 30, 0);
// or parse: date.parseDateTimeLocal("2025-03-05 21:30:00").value;

// Store `when` (UTC) and compare to date.now()/sunset() etc.
if (date.isAfter(date.now(), when)) {
    Serial.println("Already passed");
}

// When showing it again, render local:
char buf[32];
date.formatLocal(when, ESPDateFormat::DateTime, buf, sizeof(buf));
Serial.printf("Scheduled for local time: %s\n", buf);
```

Sunrise/sunset use your configured TZ (or system TZ) to compute the correct local event, but they return a UTC-backed `DateTime`. Use `formatLocal`/`toLocal` to display those events in local time.

## Date & Time Model
`DateTime` is a small value type representing a moment in time, backed by seconds since the Unix epoch:

```cpp
struct DateTime {
    int64_t epochSeconds;   // seconds since 1970-01-01T00:00:00Z

    int yearUtc() const;
    int monthUtc() const;   // 1..12
    int dayUtc() const;     // 1..31
    int hourUtc() const;    // 0..23
    int minuteUtc() const;  // 0..59
    int secondUtc() const;  // 0..59

    bool utcString(char* outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    bool localString(char* outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string utcString(ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string localString(ESPDateFormat style = ESPDateFormat::DateTime) const;
};
```

It is cheap to copy (just an `int64_t`), safe to compare and subtract, and convertible to/from `struct tm` internally by ESPDate. You never manipulate `struct tm` directly—always go through ESPDate.

```cpp
DateTime lastYear = date.subYears(1);
char buf[32];
if (lastYear.localString(buf, sizeof(buf))) {
    Serial.printf("Last year local: %s\n", buf);
}
```

## API Overview
The main module-type class you will use:

```cpp
class ESPDate {
public:
    using NtpSyncCallback = void (*)(const DateTime& syncedAtUtc);
    using NtpSyncCallable = std::function<void(const DateTime& syncedAtUtc)>;

    ~ESPDate();
    void init(const ESPDateConfig &config);
    void deinit();
    bool isInitialized() const;
    void setNtpSyncCallback(NtpSyncCallback callback);
    template <typename Callable>
    void setNtpSyncCallback(Callable&& callback); // capturing lambda/std::bind/functor
    bool setNtpSyncIntervalMs(uint32_t intervalMs);
    bool hasLastNtpSync() const;
    DateTime lastNtpSync() const;
    bool syncNTP();

    bool dateTimeToStringUtc(const DateTime &dt, char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    bool dateTimeToStringLocal(const DateTime &dt, char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    bool localDateTimeToString(const LocalDateTime &dt, char *outBuffer, size_t outSize) const;
    bool nowUtcString(char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    bool nowLocalString(char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    bool lastNtpSyncStringUtc(char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
    bool lastNtpSyncStringLocal(char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;

    std::string dateTimeToStringUtc(const DateTime &dt, ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string dateTimeToStringLocal(const DateTime &dt, ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string localDateTimeToString(const LocalDateTime &dt) const;
    std::string nowUtcString(ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string nowLocalString(ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string lastNtpSyncStringUtc(ESPDateFormat style = ESPDateFormat::DateTime) const;
    std::string lastNtpSyncStringLocal(ESPDateFormat style = ESPDateFormat::DateTime) const;

    // Time sources
    DateTime now() const;
    DateTime fromUnixSeconds(int64_t seconds) const;
    DateTime fromUtc(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) const;
    DateTime fromLocal(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) const;
    int64_t toUnixSeconds(const DateTime &dt) const;

    // Arithmetic (UTC-backed)
    DateTime addSeconds(const DateTime &dt, int64_t seconds) const;
    DateTime addMinutes(const DateTime &dt, int64_t minutes) const;
    DateTime addHours(const DateTime &dt, int64_t hours) const;
    DateTime addDays(const DateTime &dt, int32_t days) const;
    DateTime addMonths(const DateTime &dt, int32_t months) const;
    DateTime addYears(const DateTime &dt, int32_t years) const;

    DateTime subSeconds(const DateTime &dt, int64_t seconds) const;
    DateTime subMinutes(const DateTime &dt, int64_t minutes) const;
    DateTime subHours(const DateTime &dt, int64_t hours) const;
    DateTime subDays(const DateTime &dt, int32_t days) const;
    DateTime subMonths(const DateTime &dt, int32_t months) const;
    DateTime subYears(const DateTime &dt, int32_t years) const;

    // Convenience: relative to now()
    DateTime addSeconds(int64_t seconds) const;
    DateTime addMinutes(int64_t minutes) const;
    DateTime addHours(int64_t hours) const;
    DateTime addDays(int32_t days) const;
    DateTime addMonths(int32_t months) const;
    DateTime addYears(int32_t years) const;

    DateTime subSeconds(int64_t seconds) const;
    DateTime subMinutes(int64_t minutes) const;
    DateTime subHours(int64_t hours) const;
    DateTime subDays(int32_t days) const;
    DateTime subMonths(int32_t months) const;
    DateTime subYears(int32_t years) const;

    // Differences & comparisons
    int64_t differenceInSeconds(const DateTime &a, const DateTime &b) const;
    int64_t differenceInMinutes(const DateTime &a, const DateTime &b) const;
    int64_t differenceInHours(const DateTime &a, const DateTime &b) const;
    int64_t differenceInDays(const DateTime &a, const DateTime &b) const;
    bool isBefore(const DateTime &a, const DateTime &b) const;
    bool isAfter(const DateTime &a, const DateTime &b) const;
    bool isEqual(const DateTime &a, const DateTime &b) const;
    bool isSameDay(const DateTime &a, const DateTime &b) const; // UTC calendar day

    // Calendar helpers (UTC and local variants)
    DateTime startOfDayUtc(const DateTime &dt) const;
    DateTime endOfDayUtc(const DateTime &dt) const;
    DateTime startOfMonthUtc(const DateTime &dt) const;
    DateTime endOfMonthUtc(const DateTime &dt) const;
    DateTime startOfDayLocal(const DateTime &dt) const;
    DateTime endOfDayLocal(const DateTime &dt) const;
    DateTime startOfMonthLocal(const DateTime &dt) const;
    DateTime endOfMonthLocal(const DateTime &dt) const;
    DateTime startOfYearUtc(const DateTime &dt) const;
    DateTime startOfYearLocal(const DateTime &dt) const;
    DateTime setTimeOfDayLocal(const DateTime &dt, int hour, int minute, int second) const;
    DateTime setTimeOfDayUtc(const DateTime &dt, int hour, int minute, int second) const;
    DateTime nextDailyAtLocal(int hour, int minute, int second, const DateTime &from) const;
    DateTime nextWeekdayAtLocal(int weekday, int hour, int minute, int second, const DateTime &from) const;

    int getYearUtc(const DateTime &dt) const;
    int getMonthUtc(const DateTime &dt) const;   // 1..12
    int getDayUtc(const DateTime &dt) const;     // 1..31
    int getWeekdayUtc(const DateTime &dt) const; // 0=Sun..6=Sat
    int getYearLocal(const DateTime &dt) const;
    int getMonthLocal(const DateTime &dt) const;
    int getDayLocal(const DateTime &dt) const;
    int getWeekdayLocal(const DateTime &dt) const;

    bool isLeapYear(int year) const;
    int  daysInMonth(int year, int month) const; // month: 1..12

    // Formatting
    bool formatUtc(const DateTime &dt, ESPDateFormat style, char *outBuffer, size_t outSize) const;
    bool formatLocal(const DateTime &dt, ESPDateFormat style, char *outBuffer, size_t outSize) const;
    bool formatWithPatternUtc(const DateTime &dt, const char *pattern, char *outBuffer, size_t outSize) const;
    bool formatWithPatternLocal(const DateTime &dt, const char *pattern, char *outBuffer, size_t outSize) const;

    struct ParseResult { bool ok; DateTime value; };
    ParseResult parseIso8601Utc(const char *str) const;           // "YYYY-MM-DDTHH:MM:SSZ"
    ParseResult parseDateTimeLocal(const char *str) const;        // "YYYY-MM-DD HH:MM:SS"
};
```

## Examples
Full sketches:
- `examples/basic_date/basic_date.ino` for broad API coverage.
- `examples/string_helpers/string_helpers.ino` for buffer + `std::string` formatting APIs (including direct `DateTime`/`LocalDateTime` methods).
- `examples/ntp_sync_tracking/ntp_sync_tracking.ino` for `syncNTP`, callback handling, and `lastNtpSyncStringLocal/Utc`.

Difference between timestamps:

```cpp
DateTime now = date.now();
DateTime yesterday = date.subDays(1);

int64_t sec = date.differenceInSeconds(now, yesterday);
int64_t min = date.differenceInMinutes(now, yesterday);
int64_t days = date.differenceInDays(now, yesterday);

Serial.printf("Δ: %lld s, %lld min, %lld days\n",
    static_cast<long long>(sec),
    static_cast<long long>(min),
    static_cast<long long>(days)
);
```

Start/end of day (local):

```cpp
DateTime now = date.now();
DateTime start = date.startOfDayLocal(now);
DateTime end   = date.endOfDayLocal(now);

char buf[32];
date.formatLocal(start, ESPDateFormat::DateTime, buf, sizeof(buf));
Serial.print("Day starts at: ");
Serial.println(buf);

date.formatLocal(end, ESPDateFormat::DateTime, buf, sizeof(buf));
Serial.print("Day ends at: ");
Serial.println(buf);
```

Calculating the next month’s billing date:

```cpp
DateTime now = date.now();
DateTime thisBilling = date.setTimeOfDayLocal(
    date.startOfMonthLocal(now),
    3, 0, 0 // 03:00 local on the 1st
); 

DateTime nextBilling = date.addMonths(thisBilling, 1);
```

Formatting standalone values without ESPDate round-trips:

```cpp
DateTime lastYear = date.subYears(1);

char localBuf[32];
if (lastYear.localString(localBuf, sizeof(localBuf))) {
  Serial.printf("Last year local: %s\n", localBuf);
}

LocalDateTime local = date.toLocal(lastYear);
std::string localString = local.localString();
Serial.printf("LocalDateTime string: %s\n", localString.c_str());
```

## Sunrise / Sunset
Bind your coordinates and TZ once via `init`, then fetch today’s sun cycle (auto-DST):

```cpp
ESPDate solar;
ESPDateConfig cfg{47.4979f, 19.0402f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org"};
cfg.ntpServer2 = "time.google.com";
cfg.ntpServer3 = "time.cloudflare.com";
solar.init(cfg);

SunCycleResult rise = solar.sunrise();                     // today, using stored config
SunCycleResult setToday = solar.sunset();                  // today, using stored config
SunCycleResult setOnDate = solar.sunset(date.fromUtc(2024, 6, 1)); // specific day

if (rise.ok) {
  char buf[32];
  solar.formatLocal(rise.value, ESPDateFormat::DateTime, buf, sizeof(buf));
  Serial.printf("Sunrise: %s\n", buf);
}
```

Or call with explicit parameters:

```cpp
// Numeric offset + DST flag
SunCycleResult nycRise = date.sunrise(40.7128f, -74.0060f, -5.0f, true, date.fromUtc(2024, 7, 1));

// POSIX TZ string (auto-DST for that zone)
SunCycleResult nycRiseTz = date.sunrise(40.7128f, -74.0060f, "EST5EDT,M3.2.0/2,M11.1.0/2");

// Daylight check (inclusive between sunrise and sunset; offsets adjust both ends)
bool isNowDay = solar.isDay();                        // uses stored config
bool isGivenDay = solar.isDay(date.fromUtc(2024, 6, 1));
bool isWithOffsets = solar.isDay(-900, -1800);        // 15 min before sunrise, 30 min before sunset
bool dstNow = solar.isDstActive();                    // stored TZ or current system TZ
bool dstForDate = date.isDstActive(
    date.fromUtc(2024, 10, 1, 12, 0, 0),
    "EST5EDT,M3.2.0/2,M11.1.0/2"
);

// Moon phase (angle in degrees, illumination 0..1)
MoonPhaseResult phase = date.moonPhase();
if (phase.ok) {
  Serial.printf("Moon angle: %d deg, illumination: %.3f\n", phase.angleDegrees, phase.illumination);
}

// Month names (UTC calendar)
const char* month = date.monthName(date.now());  // e.g., "March"
```

When the sun never rises/sets for that day (e.g., polar regions), `ok` will be `false`.

## Scheduler-friendly helpers
- Compute the next local run at HH:MM:SS, rolling to tomorrow if needed:

```cpp
DateTime now = date.now();
DateTime nextRun = date.nextDailyAtLocal(3, 0, 0, now); // next 03:00 local
```

- Compute the next Monday 09:30 local (weekday: 1 = Monday):

```cpp
DateTime nextMonday = date.nextWeekdayAtLocal(1, 9, 30, 0, now);
```

- Truncate to the start of a period:

```cpp
DateTime startDay  = date.startOfDayLocal(now);
DateTime startYear = date.startOfYearLocal(now);
```

### Sun cycle example
See `examples/sun_cycle/sun_cycle.ino` for a full sketch. Key bits:

```cpp
ESPDate solar;
ESPDateConfig cfg{47.4979f, 19.0402f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org"};
cfg.ntpServer2 = "time.google.com";
cfg.ntpServer3 = "time.cloudflare.com";
solar.init(cfg); // call in setup after WiFi
DateTime today = solar.now();

SunCycleResult rise = solar.sunrise(today);
SunCycleResult set = solar.sunset(today);

if (rise.ok && set.ok) {
  char buf[32];
  solar.formatLocal(rise.value, ESPDateFormat::DateTime, buf, sizeof(buf));
  Serial.printf("Sunrise: %s\n", buf);
  solar.formatLocal(set.value, ESPDateFormat::DateTime, buf, sizeof(buf));
  Serial.printf("Sunset : %s\n", buf);
}
```

## Gotchas
- ESPDate configures SNTP only when you call `init` with `timeZone` and at least one configured NTP server (`ntpServer`, `ntpServer2`, or `ntpServer3`) in `ESPDateConfig` (it calls `configTzTime`). Empty server strings are ignored and compacted. Call it after WiFi is up, or ensure the device clock is set before calling `now()`. Sunrise/sunset use either the stored TZ string (if provided) or the current process TZ; make sure it matches the coordinates you pass.
- All arithmetic and comparisons are UTC-first. Local helpers rely on the current process TZ (`setenv("TZ", ...)`, `tzset()`); make sure that matches your deployment.
- Month/year arithmetic clamps to the last valid day of the target month (e.g., Jan 31 + 1 month → Feb 28/29; Feb 29 - 1 year → Feb 28).
- `differenceInDays` is purely `seconds / 86400` truncated toward zero, not a calendar-boundary delta.
- Leap seconds are treated like 60th seconds in parsing; they are not modeled beyond that.
- `isSameDay` compares the UTC calendar day. Use `startOfDayLocal` / `endOfDayLocal` if you need local-day comparisons.
- Buffer-first formatting APIs avoid extra dynamic formatting allocations and return `false` when buffers are too small or conversion fails.
- `usePSRAMBuffers` affects ESPDate-owned text buffers only; `std::string` convenience return values and callback captures may still allocate through toolchain/STL defaults.
- ESP32 toolchains typically ship a 64-bit `time_t`; on 32-bit `time_t` toolchains dates beyond 2038 may overflow (a compile-time warning is emitted).
- `differenceInDays(a, b)` is defined as `floor((a - b) / 86400)` on UTC seconds, not calendar boundaries.
- `SunCycleResult.ok` is `false` when there is no sunrise/sunset for the given day/coordinates (e.g., polar night/day).

## Restrictions
- ESP32 + FreeRTOS (Arduino-ESP32 or ESP-IDF) with C++17 enabled.
- Requires a working system clock (`time()`) and relies on POSIX-style TZ handling for local-time helpers.

## Tests
- CI builds examples via PlatformIO and Arduino CLI on common ESP32 boards to ensure the API compiles cleanly under ArduinoJson-installed environments.
- When using Arduino CLI locally, mirror CI by priming the ESP32 board manager URL before installing the core:
  ```bash
  arduino-cli config init --overwrite
  arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  arduino-cli core install esp32:esp32@3.3.3 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- You can also run `pio ci examples/basic_date --board esp32dev --project-option "build_flags=-std=gnu++17"` locally.
- Unity smoke tests live in `test/test_esp_date`; run them on hardware with `pio test -e esp32dev` (or your board environment) to exercise arithmetic, formatting, and parsing routines.

## License
MIT — see [LICENSE.md](LICENSE.md).

## ESPToolKit
- Check out other libraries: <https://github.com/orgs/ESPToolKit/repositories>
- Support the project: <https://ko-fi.com/esptoolkit>
- Visit the website: <https://www.esptoolkit.hu/>
