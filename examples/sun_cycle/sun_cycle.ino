#include <Arduino.h>
#include <ESPDate.h>

// Configure coordinates and TZ once in setup for repeated use (with NTP sync via configTzTime)
ESPDate solar;

void printLocal(const char* label, const DateTime& dt) {
  char buf[32];
  if (solar.formatLocal(dt, ESPDateFormat::DateTime, buf, sizeof(buf))) {
    Serial.print(label);
    Serial.println(buf);
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("ESPDate sun cycle example");
  Serial.println("Connect WiFi so configTzTime can sync time, or set system clock/TZ manually before running.");
  ESPDateConfig cfg{47.4979f, 19.0402f, "CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org"};
  cfg.ntpServer2 = "time.google.com";
  cfg.ntpServer3 = "time.cloudflare.com";
  solar.init(cfg);

  DateTime today = solar.now();

  SunCycleResult rise = solar.sunrise(today);
  SunCycleResult set = solar.sunset(today);

  if (rise.ok && set.ok) {
    printLocal("Sunrise: ", rise.value);
    printLocal("Sunset : ", set.value);
  } else {
    Serial.println("No sunrise/sunset for these coordinates today.");
  }

  MoonPhaseResult phase = solar.moonPhase(today);
  if (phase.ok) {
    Serial.printf("Moon phase: %d deg, illumination: %.3f\n", phase.angleDegrees, phase.illumination);
  }

  // Use offsets to extend the daylight window (e.g., start 15 min earlier, end 30 min earlier)
  bool isDayWithOffsets = solar.isDay(-900, -1800, today);
  Serial.printf("Is day (with offsets): %s\n", isDayWithOffsets ? "yes" : "no");

  // Manual call with explicit parameters (New York, auto-DST via TZ string)
  SunCycleResult nycRise = solar.sunrise(40.7128f, -74.0060f, "EST5EDT,M3.2.0/2,M11.1.0/2", today);
  if (nycRise.ok) {
    printLocal("NYC Sunrise (local TZ set for solar): ", nycRise.value);
  }
}

void loop() {
  // no-op
}
