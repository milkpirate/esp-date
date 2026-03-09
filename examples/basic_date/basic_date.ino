#include <Arduino.h>
#include <ESPDate.h>
#include <functional>

ESPDate date;
bool releasedDateResources = false;

class SyncObserver {
 public:
  void onNtpSync(const DateTime &syncedAtUtc) {
    Serial.printf("NTP synced, epoch: %lld\n", static_cast<long long>(syncedAtUtc.epochSeconds));
  }
};

SyncObserver syncObserver;

void printFormatted(const char *label, const DateTime &dt) {
  char buf[32];
  if (date.formatUtc(dt, ESPDateFormat::Iso8601, buf, sizeof(buf))) {
    Serial.print(label);
    Serial.println(buf);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESPDate basic example");
  Serial.println("Connect WiFi so configTzTime can sync time, or set the system clock manually before using date.now().");
  ESPDateConfig cfg{0.0f, 0.0f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", 15 * 60 * 1000};
  cfg.ntpServer2 = "time.google.com";
  cfg.ntpServer3 = "time.cloudflare.com";
  date.init(cfg);
  date.setNtpSyncCallback(std::bind(&SyncObserver::onNtpSync, &syncObserver, std::placeholders::_1));
  date.setNtpSyncIntervalMs(10 * 60 * 1000);  // optional runtime update
  date.syncNTP();  // force an immediate refresh using configured NTP

  DateTime now = date.now();
  DateTime tomorrow = date.addDays(now, 1);
  DateTime lastWeek = date.subDays(now, 7);
  DateTime lastYear = date.subYears(now, 1);

  printFormatted("Now (UTC): ", now);
  printFormatted("Tomorrow (UTC): ", tomorrow);
  printFormatted("Last week (UTC): ", lastWeek);

  char localNowBuffer[32];
  if (date.nowLocalString(localNowBuffer, sizeof(localNowBuffer))) {
    Serial.printf("Now (Local string): %s\n", localNowBuffer);
  }
  std::string utcNowString = date.nowUtcString();
  Serial.printf("Now (UTC string): %s\n", utcNowString.c_str());

  char lastYearLocalBuffer[32];
  if (lastYear.localString(lastYearLocalBuffer, sizeof(lastYearLocalBuffer))) {
    Serial.printf("Last year (Local string): %s\n", lastYearLocalBuffer);
  }

  int64_t deltaDays = date.differenceInDays(tomorrow, now);
  Serial.printf("Days between now and tomorrow: %lld\n", static_cast<long long>(deltaDays));

  bool equalSeconds = date.isEqual(now, tomorrow);
  bool equalMinutes = date.isEqualMinutes(now, date.addSeconds(now, 30));  // same minute
  Serial.printf("Equal (seconds precision): %s\n", equalSeconds ? "true" : "false");
  Serial.printf("Equal (minutes precision): %s\n", equalMinutes ? "true" : "false");

  DateTime start = date.startOfDayLocal(now);
  DateTime end = date.endOfDayLocal(now);

  char buf[32];
  if (date.formatLocal(start, ESPDateFormat::DateTime, buf, sizeof(buf))) {
    Serial.print("Local day starts at: ");
    Serial.println(buf);
  }
  if (date.formatLocal(end, ESPDateFormat::DateTime, buf, sizeof(buf))) {
    Serial.print("Local day ends at: ");
    Serial.println(buf);
  }

  ESPDate::ParseResult parsed = date.parseIso8601Utc("2025-12-31T23:59:30Z");
  if (parsed.ok) {
    Serial.print("Parsed ISO-8601 (UTC): ");
    date.formatUtc(parsed.value, ESPDateFormat::DateTime, buf, sizeof(buf));
    Serial.println(buf);
  }

  ESPDate::ParseResult parsedLocal = date.parseDateTimeLocal("2025-01-01 03:00:00");
  if (parsedLocal.ok) {
    Serial.print("Parsed local datetime: ");
    if (date.formatLocal(parsedLocal.value, ESPDateFormat::DateTime, buf, sizeof(buf))) {
      Serial.println(buf);
    }
  }

  MoonPhaseResult phase = date.moonPhase();
  if (phase.ok) {
    Serial.printf("Moon phase: %d deg, illumination: %.3f\n", phase.angleDegrees, phase.illumination);
  }
}

void loop() {
  // Demonstrate explicit teardown in long-running sketches.
  if (!releasedDateResources && millis() > 60000UL && date.isInitialized()) {
    date.deinit();
    releasedDateResources = true;
    Serial.println("ESPDate deinitialized.");
  }
}
