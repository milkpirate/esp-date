#include <Arduino.h>
#include <ESPDate.h>

ESPDate date;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("ESPDate string helpers example");

	// Local formatting methods use current TZ. Here we use UTC for deterministic output.
	date.init(ESPDateConfig{0.0f, 0.0f, "UTC0", nullptr});

	DateTime release = date.fromUtc(2025, 1, 2, 3, 4, 5);

	char buf[32];
	if (release.utcString(buf, sizeof(buf))) {
		Serial.printf("DateTime::utcString  -> %s\n", buf);
	}
	if (release.localString(buf, sizeof(buf))) {
		Serial.printf("DateTime::localString -> %s\n", buf);
	}

	std::string localStr = release.localString();
	Serial.printf("DateTime local std::string -> %s\n", localStr.c_str());

	// Convert to local components, then format directly from LocalDateTime.
	LocalDateTime local = date.toLocal(release);
	if (local.localString(buf, sizeof(buf))) {
		Serial.printf("LocalDateTime::localString -> %s\n", buf);
	}

	// ESPDate wrapper methods still available when preferred.
	if (date.dateTimeToStringUtc(release, buf, sizeof(buf), ESPDateFormat::Iso8601)) {
		Serial.printf("ESPDate::dateTimeToStringUtc -> %s\n", buf);
	}

	if (date.nowLocalString(buf, sizeof(buf))) {
		Serial.printf("ESPDate::nowLocalString -> %s\n", buf);
	}

	std::string nowUtc = date.nowUtcString(ESPDateFormat::Iso8601);
	Serial.printf("ESPDate::nowUtcString(ISO) -> %s\n", nowUtc.c_str());
}

void loop() {
	// no-op
}
