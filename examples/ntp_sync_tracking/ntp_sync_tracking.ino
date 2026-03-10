#include <Arduino.h>
#include <ESPDate.h>

ESPDate date;

unsigned long lastPrintMs = 0;

void printLastSync() {
	char localBuf[32];
	char utcBuf[32];

	if (!date.hasLastNtpSync()) {
		Serial.println("NTP has not synced yet.");
		return;
	}

	if (date.lastNtpSyncStringLocal(localBuf, sizeof(localBuf))) {
		Serial.printf("Last NTP sync (local): %s\n", localBuf);
	}
	if (date.lastNtpSyncStringUtc(utcBuf, sizeof(utcBuf), ESPDateFormat::Iso8601)) {
		Serial.printf("Last NTP sync (UTC)  : %s\n", utcBuf);
	}
}

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("ESPDate NTP sync tracking example");
	Serial.println(
	    "Connect WiFi before this sketch runs so SNTP can reach the configured servers."
	);

	ESPDateConfig cfg{0.0f, 0.0f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", 15 * 60 * 1000};
	cfg.ntpServer2 = "time.google.com";
	cfg.ntpServer3 = "time.cloudflare.com";
	date.init(cfg);
	date.setNtpSyncCallback([](const DateTime &syncedAtUtc) {
		char buf[32];
		if (syncedAtUtc.localString(buf, sizeof(buf))) {
			Serial.printf("Callback: synced at %s\n", buf);
		}
	});

	// Force an immediate sync attempt.
	if (!date.syncNTP()) {
		Serial.println("syncNTP() failed (missing NTP config or runtime support).\n");
	}

	printLastSync();
	lastPrintMs = millis();
}

void loop() {
	// Re-print every 30 seconds so you can observe last sync state.
	if (millis() - lastPrintMs >= 30000UL) {
		printLastSync();
		lastPrintMs = millis();
	}
}
