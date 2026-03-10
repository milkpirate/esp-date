#pragma once

#include "date_allocator.h"
#include <Arduino.h>
#include <functional>
#include <stdint.h>
#include <string>
#include <time.h>
#include <type_traits>
#include <utility>

struct timeval;

enum class ESPDateFormat { Iso8601, DateTime, Date, Time };

struct DateTime {
	int64_t epochSeconds = 0; // seconds since 1970-01-01T00:00:00Z

	int yearUtc() const;
	int monthUtc() const;  // 1..12
	int dayUtc() const;    // 1..31
	int hourUtc() const;   // 0..23
	int minuteUtc() const; // 0..59
	int secondUtc() const; // 0..59

	// Uses current system TZ for local formatting.
	bool
	utcString(char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime) const;
	bool localString(
	    char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime
	) const;
	std::string utcString(ESPDateFormat style = ESPDateFormat::DateTime) const;
	std::string localString(ESPDateFormat style = ESPDateFormat::DateTime) const;
};

struct LocalDateTime {
	bool ok = false;
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
	int second = 0;
	int offsetMinutes = 0; // local - UTC
	DateTime utc{};

	bool localString(char *outBuffer, size_t outSize) const;
	std::string localString() const;
};

struct ESPDateConfig {
	float latitude = 0.0f;
	float longitude = 0.0f;
	const char *timeZone = nullptr; // POSIX TZ string, e.g. "CET-1CEST,M3.5.0/2,M10.5.0/3"
	const char *ntpServer =
	    nullptr; // optional primary NTP server; used with timeZone to call configTzTime
	uint32_t ntpSyncIntervalMs = 0; // optional SNTP sync interval override; 0 keeps runtime default
	bool usePSRAMBuffers = false;   // prefer PSRAM for ESPDate-owned config/state text buffers
	const char *ntpServer2 = nullptr; // optional secondary NTP server
	const char *ntpServer3 = nullptr; // optional tertiary NTP server
};

struct SunCycleResult {
	bool ok;
	DateTime value;
};

struct MoonPhaseResult {
	bool ok;
	int angleDegrees;    // 0..360
	double illumination; // 0.0..1.0
};

class ESPDate {
  public:
	using NtpSyncCallback = void (*)(const DateTime &syncedAtUtc);
	using NtpSyncCallable = std::function<void(const DateTime &syncedAtUtc)>;

	ESPDate();
	~ESPDate();
	void init(const ESPDateConfig &config);
	void deinit();
	bool isInitialized() const {
		return initialized_;
	}
	// Optional SNTP sync notification. Pass nullptr to clear.
	void setNtpSyncCallback(NtpSyncCallback callback);
	// Accepts capturing lambdas / std::bind / functors.
	// Non-capturing lambdas bind to the function-pointer overload above.
	template <
	    typename Callable,
	    typename std::enable_if<
	        !std::is_convertible<typename std::decay<Callable>::type, NtpSyncCallback>::value,
	        int>::type = 0>
	void setNtpSyncCallback(Callable &&callback) {
		setNtpSyncCallbackCallable(NtpSyncCallable(std::forward<Callable>(callback)));
	}
	// Adjusts SNTP sync interval in milliseconds. Pass 0 to keep the runtime default.
	// Returns false when the runtime does not expose interval control.
	bool setNtpSyncIntervalMs(uint32_t intervalMs);
	// True after at least one successful SNTP sync callback was received.
	bool hasLastNtpSync() const;
	// Returns the last SNTP sync timestamp (UTC epoch-backed DateTime).
	// When hasLastNtpSync() is false this returns DateTime{}.
	DateTime lastNtpSync() const;
	// Triggers an immediate NTP sync with the configured server list.
	// Returns false when no NTP server is configured or SNTP runtime support is unavailable.
	bool syncNTP();

	DateTime now() const;
	DateTime nowUtc() const; // alias of now(), returns the raw system clock (UTC)
	LocalDateTime nowLocal() const;
	LocalDateTime toLocal(const DateTime &dt) const;
	LocalDateTime toLocal(const DateTime &dt, const char *timeZone) const;
	DateTime fromUnixSeconds(int64_t seconds) const;
	DateTime
	fromUtc(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) const;
	DateTime
	fromLocal(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) const;
	int64_t toUnixSeconds(const DateTime &dt) const;

	// Arithmetic relative to a provided DateTime
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

	// Convenience arithmetic relative to now()
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

	// Differences
	int64_t differenceInSeconds(const DateTime &a, const DateTime &b) const;
	int64_t differenceInMinutes(const DateTime &a, const DateTime &b) const;
	int64_t differenceInHours(const DateTime &a, const DateTime &b) const;
	int64_t differenceInDays(const DateTime &a, const DateTime &b) const;

	// Comparisons
	bool isBefore(const DateTime &a, const DateTime &b) const;
	bool isAfter(const DateTime &a, const DateTime &b) const;
	bool isEqual(const DateTime &a, const DateTime &b) const; // seconds precision
	bool isEqualMinutes(
	    const DateTime &a, const DateTime &b
	) const; // minutes precision (UTC epoch / 60)
	bool isEqualMinutesUtc(
	    const DateTime &a, const DateTime &b
	) const; // alias for minute-level UTC compare
	bool isSameDay(const DateTime &a, const DateTime &b) const;

	// Calendar helpers (UTC)
	DateTime startOfDayUtc(const DateTime &dt) const;
	DateTime endOfDayUtc(const DateTime &dt) const;
	DateTime startOfMonthUtc(const DateTime &dt) const;
	DateTime endOfMonthUtc(const DateTime &dt) const;

	int getYearUtc(const DateTime &dt) const;
	int getMonthUtc(const DateTime &dt) const;   // 1..12
	int getDayUtc(const DateTime &dt) const;     // 1..31
	int getWeekdayUtc(const DateTime &dt) const; // 0=Sunday..6=Saturday

	// Local time helpers (respect TZ)
	DateTime startOfDayLocal(const DateTime &dt) const;
	DateTime endOfDayLocal(const DateTime &dt) const;
	DateTime startOfMonthLocal(const DateTime &dt) const;
	DateTime endOfMonthLocal(const DateTime &dt) const;
	DateTime startOfYearUtc(const DateTime &dt) const;
	DateTime startOfYearLocal(const DateTime &dt) const;

	DateTime setTimeOfDayLocal(const DateTime &dt, int hour, int minute, int second) const;
	DateTime setTimeOfDayUtc(const DateTime &dt, int hour, int minute, int second) const;
	DateTime nextDailyAtLocal(int hour, int minute, int second, const DateTime &from) const;
	DateTime
	nextWeekdayAtLocal(int weekday, int hour, int minute, int second, const DateTime &from) const;

	int getYearLocal(const DateTime &dt) const;
	int getMonthLocal(const DateTime &dt) const;   // 1..12
	int getDayLocal(const DateTime &dt) const;     // 1..31
	int getWeekdayLocal(const DateTime &dt) const; // 0=Sunday..6=Saturday

	bool isLeapYear(int year) const;
	int daysInMonth(int year, int month) const; // month: 1..12

	// Formatting
	bool formatUtc(const DateTime &dt, ESPDateFormat style, char *outBuffer, size_t outSize) const;
	bool
	formatLocal(const DateTime &dt, ESPDateFormat style, char *outBuffer, size_t outSize) const;
	bool formatWithPatternUtc(
	    const DateTime &dt, const char *pattern, char *outBuffer, size_t outSize
	) const;
	bool formatWithPatternLocal(
	    const DateTime &dt, const char *pattern, char *outBuffer, size_t outSize
	) const;

	// String helpers (embedded-safe buffer first, then std::string convenience)
	bool dateTimeToStringUtc(
	    const DateTime &dt,
	    char *outBuffer,
	    size_t outSize,
	    ESPDateFormat style = ESPDateFormat::DateTime
	) const;
	bool dateTimeToStringLocal(
	    const DateTime &dt,
	    char *outBuffer,
	    size_t outSize,
	    ESPDateFormat style = ESPDateFormat::DateTime
	) const;
	bool localDateTimeToString(const LocalDateTime &dt, char *outBuffer, size_t outSize) const;
	bool nowUtcString(
	    char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime
	) const;
	bool nowLocalString(
	    char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime
	) const;
	bool lastNtpSyncStringUtc(
	    char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime
	) const;
	bool lastNtpSyncStringLocal(
	    char *outBuffer, size_t outSize, ESPDateFormat style = ESPDateFormat::DateTime
	) const;

	std::string
	dateTimeToStringUtc(const DateTime &dt, ESPDateFormat style = ESPDateFormat::DateTime) const;
	std::string
	dateTimeToStringLocal(const DateTime &dt, ESPDateFormat style = ESPDateFormat::DateTime) const;
	std::string localDateTimeToString(const LocalDateTime &dt) const;
	std::string nowUtcString(ESPDateFormat style = ESPDateFormat::DateTime) const;
	std::string nowLocalString(ESPDateFormat style = ESPDateFormat::DateTime) const;
	std::string lastNtpSyncStringUtc(ESPDateFormat style = ESPDateFormat::DateTime) const;
	std::string lastNtpSyncStringLocal(ESPDateFormat style = ESPDateFormat::DateTime) const;

	struct ParseResult {
		bool ok;
		DateTime value;
	};

	ParseResult parseIso8601Utc(const char *str) const;
	ParseResult parseDateTimeLocal(const char *str) const;

	// Sun cycle using stored configuration (lat/lon/timezone)
	SunCycleResult sunrise() const;
	SunCycleResult sunset() const;
	SunCycleResult sunrise(const DateTime &day) const;
	SunCycleResult sunset(const DateTime &day) const;

	// Sun cycle with explicit parameters (timezone in hours, DST flag)
	SunCycleResult sunrise(float latitude, float longitude, float timezoneHours, bool isDst) const;
	SunCycleResult sunset(float latitude, float longitude, float timezoneHours, bool isDst) const;
	SunCycleResult sunrise(
	    float latitude, float longitude, float timezoneHours, bool isDst, const DateTime &day
	) const;
	SunCycleResult sunset(
	    float latitude, float longitude, float timezoneHours, bool isDst, const DateTime &day
	) const;

	// Sun cycle using a POSIX TZ string (auto-DST) instead of numeric offset
	SunCycleResult sunrise(float latitude, float longitude, const char *timeZone) const;
	SunCycleResult sunset(float latitude, float longitude, const char *timeZone) const;
	SunCycleResult
	sunrise(float latitude, float longitude, const char *timeZone, const DateTime &day) const;
	SunCycleResult
	sunset(float latitude, float longitude, const char *timeZone, const DateTime &day) const;

	// Daylight checks using stored configuration
	bool isDay() const;
	bool isDay(const DateTime &day) const;
	bool isDay(int sunRiseOffsetSec, int sunSetOffsetSec) const;
	bool isDay(int sunRiseOffsetSec, int sunSetOffsetSec, const DateTime &day) const;

	// Daylight saving time helpers
	bool isDstActive() const;
	bool isDstActive(const DateTime &dt) const;
	bool isDstActive(const char *timeZone) const;
	bool isDstActive(const DateTime &dt, const char *timeZone) const;

	// Moon phase
	MoonPhaseResult moonPhase() const;
	MoonPhaseResult moonPhase(const DateTime &dt) const;

	// Month names
	const char *
	monthName(int month) const; // 1..12, returns "January" ..."December" or nullptr on invalid
	const char *monthName(const DateTime &dt) const;

  private:
#if defined(__has_include)
#if __has_include(<esp_sntp.h>)
	static void handleSntpSync(struct timeval *tv);
#endif
#endif
	void setNtpSyncCallbackCallable(const NtpSyncCallable &callback);
	bool applyNtpConfig() const;
	bool hasAnyNtpServerConfigured() const;

	SunCycleResult sunriseFromConfig(const DateTime &day) const;
	SunCycleResult sunsetFromConfig(const DateTime &day) const;
	bool isDayWithOffsets(const DateTime &day, int sunRiseOffsetSec, int sunSetOffsetSec) const;

	float latitude_ = 0.0f;
	float longitude_ = 0.0f;
	DateString timeZone_;
	static constexpr size_t kMaxNtpServers = 3;
	DateString ntpServers_[kMaxNtpServers];
	uint32_t ntpSyncIntervalMs_ = 0;
	bool usePSRAMBuffers_ = false;
	DateTime lastNtpSync_{};
	bool hasLastNtpSync_ = false;
	NtpSyncCallback ntpSyncCallback_ = nullptr;
	NtpSyncCallable ntpSyncCallbackCallable_;
	static NtpSyncCallback activeNtpSyncCallback_;
	static NtpSyncCallable activeNtpSyncCallbackCallable_;
	static ESPDate *activeNtpSyncOwner_;
	bool hasLocation_ = false;
	bool initialized_ = false;
};
