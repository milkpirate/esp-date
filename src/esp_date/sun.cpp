#include "date.h"
#include "utils.h"

#include <cmath>

using Utils = ESPDateUtils;

namespace {

bool validCoordinates(float latitude, float longitude) {
	return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -90.0f &&
	       latitude <= 90.0f && longitude >= -180.0f && longitude <= 180.0f;
}

struct LocalDateResult {
	int year = 0;
	int month = 0;
	int day = 0;
	bool ok = false;
};

struct OffsetDateResult {
	double offsetMinutes = 0.0;
	LocalDateResult date{};
};

LocalDateResult deriveLocalDateWithOffset(const DateTime &dt, int offsetSeconds) {
	int64_t shifted = dt.epochSeconds + static_cast<int64_t>(offsetSeconds);
	time_t raw = static_cast<time_t>(shifted);
	tm t{};
	if (gmtime_r(&raw, &t) == nullptr) {
		return {};
	}
	return LocalDateResult{t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, true};
}

OffsetDateResult
computeOffsetAndDate(const DateTime &dt, const char *timeZone, bool usePSRAMBuffers) {
	Utils::ScopedTz scoped(timeZone, usePSRAMBuffers);
	time_t raw = static_cast<time_t>(dt.epochSeconds);
	tm local{};
	if (localtime_r(&raw, &local) == nullptr) {
		return {};
	}

	const int64_t offsetSeconds = Utils::timegm64(local) - static_cast<int64_t>(raw);
	OffsetDateResult result;
	result.offsetMinutes = static_cast<double>(offsetSeconds) / 60.0;
	result.date = LocalDateResult{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, true};
	return result;
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kSunAngle = 90.833; // standard atmospheric refraction angle in degrees

double radToDeg(double rad) {
	return 180.0 * rad / kPi;
}

double degToRad(double deg) {
	return kPi * deg / 180.0;
}

double jDay(int year, int month, int day) {
	if (month <= 2) {
		year -= 1;
		month += 12;
	}

	int A = static_cast<int>(std::floor(year / 100.0));
	int B = 2 - A + static_cast<int>(std::floor(A / 4.0));
	return std::floor(365.25 * (year + 4716)) + std::floor(30.6001 * (month + 1)) + day + B -
	       1524.5;
}

double fractionOfCentury(double jd) {
	return (jd - 2451545.0) / 36525.0;
}

double geomMeanLongSun(double t) {
	double L0 = 280.46646 + t * (36000.76983 + t * 0.0003032);
	while (L0 > 360.0) {
		L0 -= 360.0;
	}
	while (L0 < 0.0) {
		L0 += 360.0;
	}
	return L0;
}

double geomMeanAnomalySun(double t) {
	return 357.52911 + t * (35999.05029 - 0.0001537 * t);
}

double eccentricityEarthOrbit(double t) {
	return 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
}

double meanObliquityOfEcliptic(double t) {
	double seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * 0.001813));
	return 23.0 + (26.0 + (seconds / 60.0)) / 60.0;
}

double obliquityCorrection(double t) {
	double e0 = meanObliquityOfEcliptic(t);
	double omega = 125.04 - 1934.136 * t;
	return e0 + 0.00256 * std::cos(degToRad(omega));
}

double sunEqOfCenter(double t) {
	double m = geomMeanAnomalySun(t);
	double mrad = degToRad(m);
	double sinm = std::sin(mrad);
	double sin2m = std::sin(mrad * 2.0);
	double sin3m = std::sin(mrad * 3.0);
	return sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) +
	       sin3m * 0.000289;
}

double sunTrueLong(double t) {
	double l0 = geomMeanLongSun(t);
	double c = sunEqOfCenter(t);
	return l0 + c;
}

double sunApparentLong(double t) {
	double o = sunTrueLong(t);
	double omega = 125.04 - 1934.136 * t;
	return o - 0.00569 - 0.00478 * std::sin(degToRad(omega));
}

double sunDeclination(double t) {
	double e = obliquityCorrection(t);
	double lambda = sunApparentLong(t);
	double sint = std::sin(degToRad(e)) * std::sin(degToRad(lambda));
	return radToDeg(std::asin(sint));
}

double equationOfTime(double t) {
	double epsilon = obliquityCorrection(t);
	double l0 = geomMeanLongSun(t);
	double e = eccentricityEarthOrbit(t);
	double m = geomMeanAnomalySun(t);

	double y = std::tan(degToRad(epsilon) / 2.0);
	y *= y;

	double sin2l0 = std::sin(2.0 * degToRad(l0));
	double sinm = std::sin(degToRad(m));
	double cos2l0 = std::cos(2.0 * degToRad(l0));
	double sin4l0 = std::sin(4.0 * degToRad(l0));
	double sin2m = std::sin(2.0 * degToRad(m));

	double etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 -
	               0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
	return radToDeg(etime) * 4.0;
}

double hourAngleSunrise(double lat, double solarDec) {
	double latRad = degToRad(lat);
	double sdRad = degToRad(solarDec);
	double haArg =
	    (std::cos(degToRad(kSunAngle)) / (std::cos(latRad) * std::cos(sdRad)) -
	     std::tan(latRad) * std::tan(sdRad));
	return std::acos(haArg);
}

double sunriseSetUTC(bool isRise, double jday, double latitude, double longitude) {
	double t = fractionOfCentury(jday);
	double eqTime = equationOfTime(t);
	double solarDec = sunDeclination(t);
	double hourAngle = hourAngleSunrise(latitude, solarDec);

	hourAngle = isRise ? hourAngle : -hourAngle;
	double delta = longitude + radToDeg(hourAngle);
	return 720.0 - (4.0 * delta) - eqTime;
}

int sunriseSetLocalMinutes(
    bool isRise,
    int year,
    int month,
    int day,
    double latitude,
    double longitude,
    double offsetMinutes
) {
	double jday = jDay(year, month, day);
	double timeUTC = sunriseSetUTC(isRise, jday, latitude, longitude);

	double newJday = jday + timeUTC / (60.0 * 24.0);
	double newTimeUTC = sunriseSetUTC(isRise, newJday, latitude, longitude);

	if (!std::isnan(newTimeUTC)) {
		double timeLocal = std::round(newTimeUTC + offsetMinutes);
		return static_cast<int>(timeLocal);
	}
	return -1;
}

SunCycleResult buildSunCycleResult(
    int minutes, double offsetMinutes, const LocalDateResult &date, const ESPDate &dateHelper
) {
	SunCycleResult result{false, DateTime{}};
	if (!date.ok || minutes < 0 || minutes >= 1440) {
		return result;
	}

	const int64_t offsetSeconds = static_cast<int64_t>(std::llround(offsetMinutes * 60.0));
	DateTime midnightUtc = dateHelper.fromUtc(date.year, date.month, date.day, 0, 0, 0);
	DateTime localMidnightUtc = dateHelper.subSeconds(midnightUtc, offsetSeconds);

	result.ok = true;
	result.value = dateHelper.addSeconds(
	    localMidnightUtc,
	    static_cast<int64_t>(minutes) * Utils::kSecondsPerMinute
	);
	return result;
}
} // namespace

SunCycleResult ESPDate::sunrise() const {
	return sunrise(now());
}

SunCycleResult ESPDate::sunset() const {
	return sunset(now());
}

SunCycleResult ESPDate::sunrise(const DateTime &day) const {
	return sunriseFromConfig(day);
}

SunCycleResult ESPDate::sunset(const DateTime &day) const {
	return sunsetFromConfig(day);
}

SunCycleResult
ESPDate::sunrise(float latitude, float longitude, float timezoneHours, bool isDst) const {
	return sunrise(latitude, longitude, timezoneHours, isDst, now());
}

SunCycleResult
ESPDate::sunset(float latitude, float longitude, float timezoneHours, bool isDst) const {
	return sunset(latitude, longitude, timezoneHours, isDst, now());
}

SunCycleResult ESPDate::sunrise(
    float latitude, float longitude, float timezoneHours, bool isDst, const DateTime &day
) const {
	if (!validCoordinates(latitude, longitude)) {
		return SunCycleResult{false, DateTime{}};
	}
	const double offsetMinutes = static_cast<double>(timezoneHours) * 60.0 + (isDst ? 60.0 : 0.0);
	const int offsetSeconds =
	    static_cast<int>(std::llround(offsetMinutes * Utils::kSecondsPerMinute));
	LocalDateResult localDate = deriveLocalDateWithOffset(day, offsetSeconds);
	if (!localDate.ok) {
		return SunCycleResult{false, DateTime{}};
	}
	const int minutes = sunriseSetLocalMinutes(
	    true,
	    localDate.year,
	    localDate.month,
	    localDate.day,
	    latitude,
	    longitude,
	    offsetMinutes
	);
	return buildSunCycleResult(minutes, offsetMinutes, localDate, *this);
}

SunCycleResult ESPDate::sunset(
    float latitude, float longitude, float timezoneHours, bool isDst, const DateTime &day
) const {
	if (!validCoordinates(latitude, longitude)) {
		return SunCycleResult{false, DateTime{}};
	}
	const double offsetMinutes = static_cast<double>(timezoneHours) * 60.0 + (isDst ? 60.0 : 0.0);
	const int offsetSeconds =
	    static_cast<int>(std::llround(offsetMinutes * Utils::kSecondsPerMinute));
	LocalDateResult localDate = deriveLocalDateWithOffset(day, offsetSeconds);
	if (!localDate.ok) {
		return SunCycleResult{false, DateTime{}};
	}
	const int minutes = sunriseSetLocalMinutes(
	    false,
	    localDate.year,
	    localDate.month,
	    localDate.day,
	    latitude,
	    longitude,
	    offsetMinutes
	);
	return buildSunCycleResult(minutes, offsetMinutes, localDate, *this);
}

SunCycleResult ESPDate::sunrise(float latitude, float longitude, const char *timeZone) const {
	return sunrise(latitude, longitude, timeZone, now());
}

SunCycleResult ESPDate::sunset(float latitude, float longitude, const char *timeZone) const {
	return sunset(latitude, longitude, timeZone, now());
}

SunCycleResult
ESPDate::sunrise(float latitude, float longitude, const char *timeZone, const DateTime &day) const {
	if (!validCoordinates(latitude, longitude)) {
		return SunCycleResult{false, DateTime{}};
	}
	OffsetDateResult data = computeOffsetAndDate(day, timeZone, usePSRAMBuffers_);
	if (!data.date.ok) {
		return SunCycleResult{false, DateTime{}};
	}
	const int minutes = sunriseSetLocalMinutes(
	    true,
	    data.date.year,
	    data.date.month,
	    data.date.day,
	    latitude,
	    longitude,
	    data.offsetMinutes
	);
	return buildSunCycleResult(minutes, data.offsetMinutes, data.date, *this);
}

SunCycleResult
ESPDate::sunset(float latitude, float longitude, const char *timeZone, const DateTime &day) const {
	if (!validCoordinates(latitude, longitude)) {
		return SunCycleResult{false, DateTime{}};
	}
	OffsetDateResult data = computeOffsetAndDate(day, timeZone, usePSRAMBuffers_);
	if (!data.date.ok) {
		return SunCycleResult{false, DateTime{}};
	}
	const int minutes = sunriseSetLocalMinutes(
	    false,
	    data.date.year,
	    data.date.month,
	    data.date.day,
	    latitude,
	    longitude,
	    data.offsetMinutes
	);
	return buildSunCycleResult(minutes, data.offsetMinutes, data.date, *this);
}

SunCycleResult ESPDate::sunriseFromConfig(const DateTime &day) const {
	if (!hasLocation_) {
		return SunCycleResult{false, DateTime{}};
	}
	if (!validCoordinates(latitude_, longitude_)) {
		return SunCycleResult{false, DateTime{}};
	}
	const char *tz = timeZone_.empty() ? nullptr : timeZone_.c_str();
	OffsetDateResult data = computeOffsetAndDate(day, tz, usePSRAMBuffers_);
	if (!data.date.ok) {
		return SunCycleResult{false, DateTime{}};
	}
	const int minutes = sunriseSetLocalMinutes(
	    true,
	    data.date.year,
	    data.date.month,
	    data.date.day,
	    latitude_,
	    longitude_,
	    data.offsetMinutes
	);
	return buildSunCycleResult(minutes, data.offsetMinutes, data.date, *this);
}

SunCycleResult ESPDate::sunsetFromConfig(const DateTime &day) const {
	if (!hasLocation_) {
		return SunCycleResult{false, DateTime{}};
	}
	if (!validCoordinates(latitude_, longitude_)) {
		return SunCycleResult{false, DateTime{}};
	}
	const char *tz = timeZone_.empty() ? nullptr : timeZone_.c_str();
	OffsetDateResult data = computeOffsetAndDate(day, tz, usePSRAMBuffers_);
	if (!data.date.ok) {
		return SunCycleResult{false, DateTime{}};
	}
	const int minutes = sunriseSetLocalMinutes(
	    false,
	    data.date.year,
	    data.date.month,
	    data.date.day,
	    latitude_,
	    longitude_,
	    data.offsetMinutes
	);
	return buildSunCycleResult(minutes, data.offsetMinutes, data.date, *this);
}

bool ESPDate::isDay() const {
	return isDayWithOffsets(now(), 0, 0);
}

bool ESPDate::isDay(const DateTime &day) const {
	return isDayWithOffsets(day, 0, 0);
}

bool ESPDate::isDay(int sunRiseOffsetSec, int sunSetOffsetSec) const {
	return isDayWithOffsets(now(), sunRiseOffsetSec, sunSetOffsetSec);
}

bool ESPDate::isDay(int sunRiseOffsetSec, int sunSetOffsetSec, const DateTime &day) const {
	return isDayWithOffsets(day, sunRiseOffsetSec, sunSetOffsetSec);
}

bool ESPDate::isDayWithOffsets(
    const DateTime &day, int sunRiseOffsetSec, int sunSetOffsetSec
) const {
	if (!hasLocation_ || !validCoordinates(latitude_, longitude_)) {
		return false;
	}

	SunCycleResult rise = sunriseFromConfig(day);
	SunCycleResult set = sunsetFromConfig(day);
	if (!rise.ok || !set.ok) {
		return false;
	}

	DateTime start = addSeconds(rise.value, sunRiseOffsetSec);
	DateTime end = addSeconds(set.value, sunSetOffsetSec);
	if (isAfter(start, end)) {
		return false;
	}

	const bool afterStart = !isBefore(day, start);
	const bool beforeEnd = !isAfter(day, end);
	return afterStart && beforeEnd;
}
