#include "date.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__has_include)
#if __has_include(<esp_sntp.h>)
#include <esp_sntp.h>
#define ESPDATE_HAS_CONFIG_TZ_TIME 1
#define ESPDATE_HAS_SNTP_NOTIFICATION_CB 1
#define ESPDATE_HAS_SNTP_SYNC_INTERVAL 1
#elif __has_include(<esp_netif_sntp.h>)
#include <esp_netif_sntp.h>
#define ESPDATE_HAS_CONFIG_TZ_TIME 1
#define ESPDATE_HAS_SNTP_NOTIFICATION_CB 0
#define ESPDATE_HAS_SNTP_SYNC_INTERVAL 0
#else
#define ESPDATE_HAS_CONFIG_TZ_TIME 0
#define ESPDATE_HAS_SNTP_NOTIFICATION_CB 0
#define ESPDATE_HAS_SNTP_SYNC_INTERVAL 0
#endif
#else
#define ESPDATE_HAS_CONFIG_TZ_TIME 0
#define ESPDATE_HAS_SNTP_NOTIFICATION_CB 0
#define ESPDATE_HAS_SNTP_SYNC_INTERVAL 0
#endif

#if defined(__SIZEOF_TIME_T__) && __SIZEOF_TIME_T__ < 8
#warning "ESPDate detected 32-bit time_t; dates beyond 2038 may overflow."
#endif

using Utils = ESPDateUtils;

namespace {
const char *patternForStyle(ESPDateFormat style, bool localIso8601) {
	switch (style) {
	case ESPDateFormat::Iso8601:
		return localIso8601 ? "%Y-%m-%dT%H:%M:%S%z" : "%Y-%m-%dT%H:%M:%SZ";
	case ESPDateFormat::DateTime:
		return "%Y-%m-%d %H:%M:%S";
	case ESPDateFormat::Date:
		return "%Y-%m-%d";
	case ESPDateFormat::Time:
		return "%H:%M:%S";
	}
	return "%Y-%m-%d %H:%M:%S";
}

bool formatWithTm(const tm &value, const char *pattern, char *outBuffer, size_t outSize) {
	if (!pattern || !outBuffer || outSize == 0) {
		return false;
	}
	tm copy = value;
	const size_t written = strftime(outBuffer, outSize, pattern, &copy);
	return written > 0;
}
} // namespace

ESPDate::NtpSyncCallback ESPDate::activeNtpSyncCallback_ = nullptr;
ESPDate::NtpSyncCallable ESPDate::activeNtpSyncCallbackCallable_{};
ESPDate *ESPDate::activeNtpSyncOwner_ = nullptr;

#if ESPDATE_HAS_SNTP_NOTIFICATION_CB
void ESPDate::handleSntpSync(struct timeval *tv) {
	int64_t syncedEpoch = static_cast<int64_t>(time(nullptr));
	if (tv) {
		syncedEpoch = static_cast<int64_t>(tv->tv_sec);
	}
	const DateTime syncedAtUtc{syncedEpoch};
	if (activeNtpSyncOwner_) {
		activeNtpSyncOwner_->lastNtpSync_ = syncedAtUtc;
		activeNtpSyncOwner_->hasLastNtpSync_ = true;
	}

	if (activeNtpSyncCallbackCallable_) {
		activeNtpSyncCallbackCallable_(syncedAtUtc);
		return;
	}
	if (activeNtpSyncCallback_) {
		activeNtpSyncCallback_(syncedAtUtc);
	}
}
#endif

int DateTime::yearUtc() const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return 0;
	}
	return t.tm_year + 1900;
}

int DateTime::monthUtc() const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return 0;
	}
	return t.tm_mon + 1;
}

int DateTime::dayUtc() const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return 0;
	}
	return t.tm_mday;
}

int DateTime::hourUtc() const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return 0;
	}
	return t.tm_hour;
}

int DateTime::minuteUtc() const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return 0;
	}
	return t.tm_min;
}

int DateTime::secondUtc() const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return 0;
	}
	return t.tm_sec;
}

bool DateTime::utcString(char *outBuffer, size_t outSize, ESPDateFormat style) const {
	tm t{};
	if (!Utils::toUtcTm(*this, t)) {
		return false;
	}
	return formatWithTm(t, patternForStyle(style, false), outBuffer, outSize);
}

bool DateTime::localString(char *outBuffer, size_t outSize, ESPDateFormat style) const {
	tm t{};
	if (!Utils::toLocalTm(*this, t)) {
		return false;
	}
	return formatWithTm(t, patternForStyle(style, true), outBuffer, outSize);
}

std::string DateTime::utcString(ESPDateFormat style) const {
	char buffer[40];
	if (!utcString(buffer, sizeof(buffer), style)) {
		return std::string();
	}
	return std::string(buffer);
}

std::string DateTime::localString(ESPDateFormat style) const {
	char buffer[48];
	if (!localString(buffer, sizeof(buffer), style)) {
		return std::string();
	}
	return std::string(buffer);
}

bool LocalDateTime::localString(char *outBuffer, size_t outSize) const {
	if (!ok || !outBuffer || outSize == 0) {
		return false;
	}
	const int written = std::snprintf(
	    outBuffer,
	    outSize,
	    "%04d-%02d-%02d %02d:%02d:%02d",
	    year,
	    month,
	    day,
	    hour,
	    minute,
	    second
	);
	return written > 0 && static_cast<size_t>(written) < outSize;
}

std::string LocalDateTime::localString() const {
	char buffer[32];
	if (!localString(buffer, sizeof(buffer))) {
		return std::string();
	}
	return std::string(buffer);
}

ESPDate::ESPDate() = default;

ESPDate::~ESPDate() {
	deinit();
}

void ESPDate::deinit() {
	ntpSyncCallback_ = nullptr;
	ntpSyncCallbackCallable_ = NtpSyncCallable{};
	hasLastNtpSync_ = false;
	lastNtpSync_ = DateTime{};
	hasLocation_ = false;
	latitude_ = 0.0f;
	longitude_ = 0.0f;
	ntpSyncIntervalMs_ = 0;
	const bool usePSRAM = usePSRAMBuffers_;
	timeZone_ = DateString(DateAllocator<char>(usePSRAM));
	for (size_t i = 0; i < kMaxNtpServers; ++i) {
		ntpServers_[i] = DateString(DateAllocator<char>(usePSRAM));
	}
	usePSRAMBuffers_ = false;
	initialized_ = false;

	if (activeNtpSyncOwner_ == this) {
		activeNtpSyncOwner_ = nullptr;
		activeNtpSyncCallback_ = nullptr;
		activeNtpSyncCallbackCallable_ = NtpSyncCallable{};
#if ESPDATE_HAS_SNTP_NOTIFICATION_CB
		sntp_set_time_sync_notification_cb(nullptr);
#endif
	}
}

void ESPDate::init(const ESPDateConfig &config) {
	latitude_ = config.latitude;
	longitude_ = config.longitude;
	hasLocation_ = true;
	usePSRAMBuffers_ = config.usePSRAMBuffers;
	timeZone_ = DateString(DateAllocator<char>(usePSRAMBuffers_));
	for (size_t i = 0; i < kMaxNtpServers; ++i) {
		ntpServers_[i] = DateString(DateAllocator<char>(usePSRAMBuffers_));
	}
	ntpSyncIntervalMs_ = config.ntpSyncIntervalMs;
	hasLastNtpSync_ = false;
	lastNtpSync_ = DateTime{};

	const bool hasTz = config.timeZone && config.timeZone[0] != '\0';
	const char *configuredNtpServers[kMaxNtpServers] =
	    {config.ntpServer, config.ntpServer2, config.ntpServer3};
	size_t ntpServerCount = 0;
	if (hasTz) {
		timeZone_ = config.timeZone;
	}
	for (size_t i = 0; i < kMaxNtpServers; ++i) {
		const char *server = configuredNtpServers[i];
		if (!server || server[0] == '\0') {
			continue;
		}
		ntpServers_[ntpServerCount++] = server;
	}

	if (!applyNtpConfig() && hasTz) {
		setenv("TZ", timeZone_.c_str(), 1);
		tzset();
	}
	initialized_ = true;
}

void ESPDate::setNtpSyncCallback(NtpSyncCallback callback) {
	activeNtpSyncOwner_ = this;
	ntpSyncCallback_ = callback;
	ntpSyncCallbackCallable_ = NtpSyncCallable{};
	activeNtpSyncCallback_ = callback;
	activeNtpSyncCallbackCallable_ = NtpSyncCallable{};
#if ESPDATE_HAS_SNTP_NOTIFICATION_CB
	const bool keepTrackingEnabled = hasAnyNtpServerConfigured();
	sntp_set_time_sync_notification_cb(
	    (callback || keepTrackingEnabled) ? &ESPDate::handleSntpSync : nullptr
	);
#endif
}

void ESPDate::setNtpSyncCallbackCallable(const NtpSyncCallable &callback) {
	activeNtpSyncOwner_ = this;
	ntpSyncCallback_ = nullptr;
	ntpSyncCallbackCallable_ = callback;
	activeNtpSyncCallback_ = nullptr;
	activeNtpSyncCallbackCallable_ = callback;
#if ESPDATE_HAS_SNTP_NOTIFICATION_CB
	const bool keepTrackingEnabled = hasAnyNtpServerConfigured();
	sntp_set_time_sync_notification_cb(
	    (static_cast<bool>(callback) || keepTrackingEnabled) ? &ESPDate::handleSntpSync : nullptr
	);
#endif
}

bool ESPDate::setNtpSyncIntervalMs(uint32_t intervalMs) {
	ntpSyncIntervalMs_ = intervalMs;
#if ESPDATE_HAS_SNTP_SYNC_INTERVAL
	if (intervalMs > 0) {
		sntp_set_sync_interval(intervalMs);
	}
	return true;
#else
	return intervalMs == 0;
#endif
}

bool ESPDate::hasLastNtpSync() const {
	return hasLastNtpSync_;
}

DateTime ESPDate::lastNtpSync() const {
	return lastNtpSync_;
}

bool ESPDate::syncNTP() {
	return applyNtpConfig();
}

bool ESPDate::hasAnyNtpServerConfigured() const {
	for (size_t i = 0; i < kMaxNtpServers; ++i) {
		if (!ntpServers_[i].empty()) {
			return true;
		}
	}
	return false;
}

bool ESPDate::applyNtpConfig() const {
#if ESPDATE_HAS_CONFIG_TZ_TIME
	if (!hasAnyNtpServerConfigured()) {
		return false;
	}
	activeNtpSyncOwner_ = const_cast<ESPDate *>(this);

#if ESPDATE_HAS_SNTP_NOTIFICATION_CB
	activeNtpSyncCallback_ = ntpSyncCallback_;
	activeNtpSyncCallbackCallable_ = ntpSyncCallbackCallable_;
	const bool hasCallback =
	    (ntpSyncCallback_ != nullptr) || static_cast<bool>(ntpSyncCallbackCallable_);
	sntp_set_time_sync_notification_cb(
	    (hasCallback || activeNtpSyncOwner_ != nullptr) ? &ESPDate::handleSntpSync : nullptr
	);
#endif
#if ESPDATE_HAS_SNTP_SYNC_INTERVAL
	if (ntpSyncIntervalMs_ > 0) {
		sntp_set_sync_interval(ntpSyncIntervalMs_);
	}
#endif

	const char *tz = timeZone_.empty() ? "UTC0" : timeZone_.c_str();
	const char *ntpServer1 = ntpServers_[0].empty() ? nullptr : ntpServers_[0].c_str();
	const char *ntpServer2 = ntpServers_[1].empty() ? nullptr : ntpServers_[1].c_str();
	const char *ntpServer3 = ntpServers_[2].empty() ? nullptr : ntpServers_[2].c_str();
	configTzTime(tz, ntpServer1, ntpServer2, ntpServer3);
	return true;
#else
	return false;
#endif
}

DateTime ESPDate::now() const {
	return DateTime{static_cast<int64_t>(time(nullptr))};
}

DateTime ESPDate::nowUtc() const {
	return now();
}

LocalDateTime ESPDate::nowLocal() const {
	return toLocal(now(), nullptr);
}

LocalDateTime ESPDate::toLocal(const DateTime &dt) const {
	return toLocal(dt, nullptr);
}

LocalDateTime ESPDate::toLocal(const DateTime &dt, const char *timeZone) const {
	LocalDateTime result{};
	const char *tz = timeZone;
	if (!tz || tz[0] == '\0') {
		tz = timeZone_.empty() ? nullptr : timeZone_.c_str();
	}

	Utils::ScopedTz scoped(tz, usePSRAMBuffers_);
	time_t raw = static_cast<time_t>(dt.epochSeconds);
	tm local{};
	if (localtime_r(&raw, &local) == nullptr) {
		return result;
	}

	const int offsetSeconds = static_cast<int>(Utils::timegm64(local) - static_cast<int64_t>(raw));
	result.ok = true;
	result.year = local.tm_year + 1900;
	result.month = local.tm_mon + 1;
	result.day = local.tm_mday;
	result.hour = local.tm_hour;
	result.minute = local.tm_min;
	result.second = local.tm_sec;
	result.offsetMinutes = offsetSeconds / 60;
	result.utc = dt;
	return result;
}

DateTime ESPDate::fromUnixSeconds(int64_t seconds) const {
	return DateTime{seconds};
}

DateTime ESPDate::fromUtc(int year, int month, int day, int hour, int minute, int second) const {
	if (!Utils::validHms(hour, minute, second) || month < 1 || month > 12 || year < 0 ||
	    year > 9999) {
		return DateTime{};
	}
	const int clampedDay = Utils::clampDay(year, month, day, *this);
	tm t{};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = clampedDay;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	t.tm_isdst = 0;
	return Utils::fromUtcTm(t);
}

DateTime ESPDate::fromLocal(int year, int month, int day, int hour, int minute, int second) const {
	if (!Utils::validHms(hour, minute, second) || month < 1 || month > 12 || year < 0 ||
	    year > 9999) {
		return DateTime{};
	}
	const int clampedDay = Utils::clampDay(year, month, day, *this);
	tm t{};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = clampedDay;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	t.tm_isdst = -1; // let the runtime figure DST
	return Utils::fromLocalTm(t);
}

int64_t ESPDate::toUnixSeconds(const DateTime &dt) const {
	return dt.epochSeconds;
}

bool ESPDate::isDstActive() const {
	return isDstActive(now());
}

bool ESPDate::isDstActive(const DateTime &dt) const {
	return isDstActive(dt, nullptr);
}

bool ESPDate::isDstActive(const char *timeZone) const {
	return isDstActive(now(), timeZone);
}

bool ESPDate::isDstActive(const DateTime &dt, const char *timeZone) const {
	const char *tz = timeZone;
	if (!tz || tz[0] == '\0') {
		if (!timeZone_.empty()) {
			tz = timeZone_.c_str();
		} else {
			tz = nullptr;
		}
	}
	return Utils::isDstActiveFor(dt, tz, usePSRAMBuffers_);
}

DateTime ESPDate::addSeconds(const DateTime &dt, int64_t seconds) const {
	return DateTime{dt.epochSeconds + seconds};
}

DateTime ESPDate::addMinutes(const DateTime &dt, int64_t minutes) const {
	return addSeconds(dt, minutes * Utils::kSecondsPerMinute);
}

DateTime ESPDate::addHours(const DateTime &dt, int64_t hours) const {
	return addSeconds(dt, hours * Utils::kSecondsPerHour);
}

DateTime ESPDate::addDays(const DateTime &dt, int32_t days) const {
	return addSeconds(dt, static_cast<int64_t>(days) * Utils::kSecondsPerDay);
}

DateTime ESPDate::addMonths(const DateTime &dt, int32_t months) const {
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return dt;
	}

	int totalMonths = t.tm_mon + months;
	int yearsDelta = totalMonths / 12;
	int newMonth = totalMonths % 12;
	if (newMonth < 0) {
		newMonth += 12;
		--yearsDelta;
	}

	t.tm_year += yearsDelta;
	t.tm_mon = newMonth;
	t.tm_mday = Utils::clampDay(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, *this);

	return Utils::fromUtcTm(t);
}

DateTime ESPDate::addYears(const DateTime &dt, int32_t years) const {
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return dt;
	}
	t.tm_year += years;
	t.tm_mday = Utils::clampDay(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, *this);
	return Utils::fromUtcTm(t);
}

DateTime ESPDate::subSeconds(const DateTime &dt, int64_t seconds) const {
	return addSeconds(dt, -seconds);
}

DateTime ESPDate::subMinutes(const DateTime &dt, int64_t minutes) const {
	return addMinutes(dt, -minutes);
}

DateTime ESPDate::subHours(const DateTime &dt, int64_t hours) const {
	return addHours(dt, -hours);
}

DateTime ESPDate::subDays(const DateTime &dt, int32_t days) const {
	return addDays(dt, -days);
}

DateTime ESPDate::subMonths(const DateTime &dt, int32_t months) const {
	return addMonths(dt, -months);
}

DateTime ESPDate::subYears(const DateTime &dt, int32_t years) const {
	return addYears(dt, -years);
}

DateTime ESPDate::addSeconds(int64_t seconds) const {
	return addSeconds(now(), seconds);
}

DateTime ESPDate::addMinutes(int64_t minutes) const {
	return addMinutes(now(), minutes);
}

DateTime ESPDate::addHours(int64_t hours) const {
	return addHours(now(), hours);
}

DateTime ESPDate::addDays(int32_t days) const {
	return addDays(now(), days);
}

DateTime ESPDate::addMonths(int32_t months) const {
	return addMonths(now(), months);
}

DateTime ESPDate::addYears(int32_t years) const {
	return addYears(now(), years);
}

DateTime ESPDate::subSeconds(int64_t seconds) const {
	return subSeconds(now(), seconds);
}

DateTime ESPDate::subMinutes(int64_t minutes) const {
	return subMinutes(now(), minutes);
}

DateTime ESPDate::subHours(int64_t hours) const {
	return subHours(now(), hours);
}

DateTime ESPDate::subDays(int32_t days) const {
	return subDays(now(), days);
}

DateTime ESPDate::subMonths(int32_t months) const {
	return subMonths(now(), months);
}

DateTime ESPDate::subYears(int32_t years) const {
	return subYears(now(), years);
}

int64_t ESPDate::differenceInSeconds(const DateTime &a, const DateTime &b) const {
	return a.epochSeconds - b.epochSeconds;
}

int64_t ESPDate::differenceInMinutes(const DateTime &a, const DateTime &b) const {
	return differenceInSeconds(a, b) / Utils::kSecondsPerMinute;
}

int64_t ESPDate::differenceInHours(const DateTime &a, const DateTime &b) const {
	return differenceInSeconds(a, b) / Utils::kSecondsPerHour;
}

int64_t ESPDate::differenceInDays(const DateTime &a, const DateTime &b) const {
	return differenceInSeconds(a, b) / Utils::kSecondsPerDay;
}

bool ESPDate::isBefore(const DateTime &a, const DateTime &b) const {
	return a.epochSeconds < b.epochSeconds;
}

bool ESPDate::isAfter(const DateTime &a, const DateTime &b) const {
	return a.epochSeconds > b.epochSeconds;
}

bool ESPDate::isEqual(const DateTime &a, const DateTime &b) const {
	return a.epochSeconds == b.epochSeconds;
}

bool ESPDate::isEqualMinutes(const DateTime &a, const DateTime &b) const {
	return (a.epochSeconds / Utils::kSecondsPerMinute) ==
	       (b.epochSeconds / Utils::kSecondsPerMinute);
}

bool ESPDate::isEqualMinutesUtc(const DateTime &a, const DateTime &b) const {
	return isEqualMinutes(a, b);
}

bool ESPDate::isSameDay(const DateTime &a, const DateTime &b) const {
	return isEqual(startOfDayUtc(a), startOfDayUtc(b));
}

DateTime ESPDate::startOfDayUtc(const DateTime &dt) const {
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return dt;
	}
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	return Utils::fromUtcTm(t);
}

DateTime ESPDate::endOfDayUtc(const DateTime &dt) const {
	return addSeconds(startOfDayUtc(dt), Utils::kSecondsPerDay - 1);
}

DateTime ESPDate::startOfMonthUtc(const DateTime &dt) const {
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return dt;
	}
	t.tm_mday = 1;
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	return Utils::fromUtcTm(t);
}

DateTime ESPDate::endOfMonthUtc(const DateTime &dt) const {
	DateTime start = startOfMonthUtc(dt);
	DateTime nextMonth = addMonths(start, 1);
	return subSeconds(nextMonth, 1);
}

int ESPDate::getYearUtc(const DateTime &dt) const {
	return dt.yearUtc();
}

int ESPDate::getMonthUtc(const DateTime &dt) const {
	return dt.monthUtc();
}

int ESPDate::getDayUtc(const DateTime &dt) const {
	return dt.dayUtc();
}

int ESPDate::getWeekdayUtc(const DateTime &dt) const {
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return 0;
	}
	return t.tm_wday;
}

DateTime ESPDate::startOfDayLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return dt;
	}
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	return Utils::fromLocalTm(t);
}

DateTime ESPDate::endOfDayLocal(const DateTime &dt) const {
	return addSeconds(startOfDayLocal(dt), Utils::kSecondsPerDay - 1);
}

DateTime ESPDate::startOfMonthLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return dt;
	}
	t.tm_mday = 1;
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	return Utils::fromLocalTm(t);
}

DateTime ESPDate::endOfMonthLocal(const DateTime &dt) const {
	DateTime start = startOfMonthLocal(dt);
	tm t{};
	if (!Utils::toLocalTm(start, t)) {
		return start;
	}
	t.tm_mon += 1;
	DateTime nextMonth = Utils::fromLocalTm(t);
	return subSeconds(nextMonth, 1);
}

DateTime ESPDate::startOfYearUtc(const DateTime &dt) const {
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return dt;
	}
	t.tm_mon = 0;
	t.tm_mday = 1;
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	return Utils::fromUtcTm(t);
}

DateTime ESPDate::startOfYearLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return dt;
	}
	t.tm_mon = 0;
	t.tm_mday = 1;
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	return Utils::fromLocalTm(t);
}

DateTime ESPDate::setTimeOfDayLocal(const DateTime &dt, int hour, int minute, int second) const {
	if (!Utils::validHms(hour, minute, second)) {
		return dt;
	}
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return dt;
	}
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	return Utils::fromLocalTm(t);
}

DateTime ESPDate::setTimeOfDayUtc(const DateTime &dt, int hour, int minute, int second) const {
	if (!Utils::validHms(hour, minute, second)) {
		return dt;
	}
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return dt;
	}
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	return Utils::fromUtcTm(t);
}

DateTime ESPDate::nextDailyAtLocal(int hour, int minute, int second, const DateTime &from) const {
	if (!Utils::validHms(hour, minute, second)) {
		return from;
	}
	DateTime candidate = setTimeOfDayLocal(from, hour, minute, second);
	if (!isAfter(from, candidate)) {
		return candidate;
	}
	DateTime nextDay = addDays(from, 1);
	return setTimeOfDayLocal(nextDay, hour, minute, second);
}

DateTime ESPDate::nextWeekdayAtLocal(
    int weekday, int hour, int minute, int second, const DateTime &from
) const {
	if (!Utils::validHms(hour, minute, second) || weekday < 0 || weekday > 6) {
		return from;
	}
	const int current = getWeekdayLocal(from);
	int daysAhead = (weekday - current + 7) % 7;
	DateTime candidateDay = addDays(from, daysAhead);
	DateTime candidate = setTimeOfDayLocal(candidateDay, hour, minute, second);
	if (daysAhead == 0 && isAfter(from, candidate)) {
		candidate = setTimeOfDayLocal(addDays(from, 7), hour, minute, second);
	}
	return candidate;
}

int ESPDate::getYearLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return 0;
	}
	return t.tm_year + 1900;
}

int ESPDate::getMonthLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return 0;
	}
	return t.tm_mon + 1;
}

int ESPDate::getDayLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return 0;
	}
	return t.tm_mday;
}

int ESPDate::getWeekdayLocal(const DateTime &dt) const {
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return 0;
	}
	return t.tm_wday;
}

bool ESPDate::isLeapYear(int year) const {
	if (year % 4 != 0) {
		return false;
	}
	if (year % 100 != 0) {
		return true;
	}
	return (year % 400) == 0;
}

int ESPDate::daysInMonth(int year, int month) const {
	static const int daysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month < 1 || month > 12) {
		return 0;
	}
	if (month == 2 && isLeapYear(year)) {
		return 29;
	}
	return daysPerMonth[month - 1];
}

bool ESPDate::formatUtc(
    const DateTime &dt, ESPDateFormat style, char *outBuffer, size_t outSize
) const {
	return formatWithPatternUtc(dt, patternForStyle(style, false), outBuffer, outSize);
}

bool ESPDate::formatLocal(
    const DateTime &dt, ESPDateFormat style, char *outBuffer, size_t outSize
) const {
	return formatWithPatternLocal(dt, patternForStyle(style, true), outBuffer, outSize);
}

bool ESPDate::formatWithPatternUtc(
    const DateTime &dt, const char *pattern, char *outBuffer, size_t outSize
) const {
	if (!pattern || !outBuffer || outSize == 0) {
		return false;
	}
	tm t{};
	if (!Utils::toUtcTm(dt, t)) {
		return false;
	}
	size_t written = strftime(outBuffer, outSize, pattern, &t);
	return written > 0;
}

bool ESPDate::formatWithPatternLocal(
    const DateTime &dt, const char *pattern, char *outBuffer, size_t outSize
) const {
	if (!pattern || !outBuffer || outSize == 0) {
		return false;
	}
	tm t{};
	if (!Utils::toLocalTm(dt, t)) {
		return false;
	}
	size_t written = strftime(outBuffer, outSize, pattern, &t);
	return written > 0;
}

bool ESPDate::dateTimeToStringUtc(
    const DateTime &dt, char *outBuffer, size_t outSize, ESPDateFormat style
) const {
	return dt.utcString(outBuffer, outSize, style);
}

bool ESPDate::dateTimeToStringLocal(
    const DateTime &dt, char *outBuffer, size_t outSize, ESPDateFormat style
) const {
	return dt.localString(outBuffer, outSize, style);
}

bool ESPDate::localDateTimeToString(
    const LocalDateTime &dt, char *outBuffer, size_t outSize
) const {
	return dt.localString(outBuffer, outSize);
}

bool ESPDate::nowUtcString(char *outBuffer, size_t outSize, ESPDateFormat style) const {
	return dateTimeToStringUtc(nowUtc(), outBuffer, outSize, style);
}

bool ESPDate::nowLocalString(char *outBuffer, size_t outSize, ESPDateFormat style) const {
	return dateTimeToStringLocal(now(), outBuffer, outSize, style);
}

bool ESPDate::lastNtpSyncStringUtc(char *outBuffer, size_t outSize, ESPDateFormat style) const {
	if (!hasLastNtpSync_) {
		return false;
	}
	return dateTimeToStringUtc(lastNtpSync_, outBuffer, outSize, style);
}

bool ESPDate::lastNtpSyncStringLocal(char *outBuffer, size_t outSize, ESPDateFormat style) const {
	if (!hasLastNtpSync_) {
		return false;
	}
	return dateTimeToStringLocal(lastNtpSync_, outBuffer, outSize, style);
}

std::string ESPDate::dateTimeToStringUtc(const DateTime &dt, ESPDateFormat style) const {
	char buffer[40];
	if (!dateTimeToStringUtc(dt, buffer, sizeof(buffer), style)) {
		return std::string();
	}
	return std::string(buffer);
}

std::string ESPDate::dateTimeToStringLocal(const DateTime &dt, ESPDateFormat style) const {
	char buffer[48];
	if (!dateTimeToStringLocal(dt, buffer, sizeof(buffer), style)) {
		return std::string();
	}
	return std::string(buffer);
}

std::string ESPDate::localDateTimeToString(const LocalDateTime &dt) const {
	char buffer[32];
	if (!localDateTimeToString(dt, buffer, sizeof(buffer))) {
		return std::string();
	}
	return std::string(buffer);
}

std::string ESPDate::nowUtcString(ESPDateFormat style) const {
	return dateTimeToStringUtc(nowUtc(), style);
}

std::string ESPDate::nowLocalString(ESPDateFormat style) const {
	return dateTimeToStringLocal(now(), style);
}

std::string ESPDate::lastNtpSyncStringUtc(ESPDateFormat style) const {
	if (!hasLastNtpSync_) {
		return std::string();
	}
	return dateTimeToStringUtc(lastNtpSync_, style);
}

std::string ESPDate::lastNtpSyncStringLocal(ESPDateFormat style) const {
	if (!hasLastNtpSync_) {
		return std::string();
	}
	return dateTimeToStringLocal(lastNtpSync_, style);
}

ESPDate::ParseResult ESPDate::parseIso8601Utc(const char *str) const {
	ParseResult result{false, DateTime{}};
	if (!str) {
		return result;
	}
	const size_t len = std::strlen(str);
	if (len != 20 || str[4] != '-' || str[7] != '-' || (str[10] != 'T' && str[10] != 't') ||
	    str[13] != ':' || str[16] != ':' || (str[19] != 'Z' && str[19] != 'z')) {
		return result;
	}

	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	if (!Utils::parseIntSlice(str, 4, 0, 9999, year) ||
	    !Utils::parseIntSlice(str + 5, 2, 1, 12, month) ||
	    !Utils::parseIntSlice(str + 8, 2, 1, 31, day) ||
	    !Utils::parseIntSlice(str + 11, 2, 0, 23, hour) ||
	    !Utils::parseIntSlice(str + 14, 2, 0, 59, minute) ||
	    !Utils::parseIntSlice(str + 17, 2, 0, 60, second)) {
		return result;
	}

	const int maxDay = daysInMonth(year, month);
	if (day > maxDay) {
		return result;
	}

	tm t{};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	t.tm_isdst = 0;

	result.ok = true;
	result.value = Utils::fromUtcTm(t);
	return result;
}

ESPDate::ParseResult ESPDate::parseDateTimeLocal(const char *str) const {
	ParseResult result{false, DateTime{}};
	if (!str) {
		return result;
	}
	const size_t len = std::strlen(str);
	if (len != 19 || str[4] != '-' || str[7] != '-' || str[10] != ' ' || str[13] != ':' ||
	    str[16] != ':') {
		return result;
	}

	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	if (!Utils::parseIntSlice(str, 4, 0, 9999, year) ||
	    !Utils::parseIntSlice(str + 5, 2, 1, 12, month) ||
	    !Utils::parseIntSlice(str + 8, 2, 1, 31, day) ||
	    !Utils::parseIntSlice(str + 11, 2, 0, 23, hour) ||
	    !Utils::parseIntSlice(str + 14, 2, 0, 59, minute) ||
	    !Utils::parseIntSlice(str + 17, 2, 0, 60, second)) {
		return result;
	}

	const int maxDay = daysInMonth(year, month);
	if (day > maxDay) {
		return result;
	}

	tm t{};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_sec = second;
	t.tm_isdst = -1; // let the runtime decide

	result.ok = true;
	result.value = Utils::fromLocalTm(t);
	return result;
}

const char *ESPDate::monthName(int month) const {
	static const char *kMonths[12] = {
	    "January",
	    "February",
	    "March",
	    "April",
	    "May",
	    "June",
	    "July",
	    "August",
	    "September",
	    "October",
	    "November",
	    "December"
	};
	if (month < 1 || month > 12) {
		return nullptr;
	}
	return kMonths[month - 1];
}

const char *ESPDate::monthName(const DateTime &dt) const {
	return monthName(dt.monthUtc());
}
