#include "DisplayManager.h"

#include <Wire.h>
#include <time.h>

#include "Config.h"

namespace {
const int16_t kBarY = 55;
const int16_t kBarHeight = 9;

void formatTarget(char *buffer, size_t size, float value) {
  if (isnan(value)) {
    snprintf(buffer, size, "   --");
  } else {
    snprintf(buffer, size, "%5.1f", value);
  }
}
}  // namespace

DisplayManager::DisplayManager() : _oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1) {}

bool DisplayManager::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // The driver does not report a missing device, so probe the bus first.
  Wire.beginTransmission(OLED_ADDRESS);
  if (Wire.endTransmission() != 0) return false;

  _available = _oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, false, false);
  if (_available) {
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextWrap(false);
  }
  return _available;
}

void DisplayManager::update(const SystemSnapshot &state, float latitude, float longitude) {
  if (!_available) return;

  char clock[6] = "--:--";
  const time_t now = time(nullptr);
  if (now >= static_cast<time_t>(MIN_VALID_EPOCH)) {
    const time_t local = now + UTC_OFFSET_HOURS * 3600;
    struct tm parts;
    gmtime_r(&local, &parts);
    snprintf(clock, sizeof(clock), "%02d:%02d", parts.tm_hour, parts.tm_min);
  }

  char target[8];
  _oled.clearDisplay();

  _oled.setCursor(0, 0);
  _oled.printf("WIFI:%s MQTT:%s %s", wifiStatusText(state.wifi), state.mqttConnected ? "OK" : "--", clock);

  // Address to open the web interface: router IP in station mode, 192.168.4.1 in access point mode.
  _oled.setCursor(0, 9);
  _oled.printf("IP: %s", state.wifi == WifiStatus::Disconnected ? "--" : state.ip);

  _oled.setCursor(0, 18);
  _oled.printf("Lat:%.2f Lon:%.2f", latitude, longitude);

  formatTarget(target, sizeof(target), state.azTarget);
  _oled.setCursor(0, 27);
  _oled.printf("Az:%6.1f /%s", state.azReal, target);

  formatTarget(target, sizeof(target), state.elTarget);
  _oled.setCursor(0, 36);
  _oled.printf("El:%6.1f /%s", state.elReal, target);

  _oled.setCursor(0, 45);
  _oled.printf("Rad: %.2f V", state.radiationV);

  const float ratio = constrain(state.radiationV / static_cast<float>(V_REF), 0.0f, 1.0f);
  _oled.drawRect(0, kBarY, OLED_WIDTH, kBarHeight, SSD1306_WHITE);
  _oled.fillRect(1, kBarY + 1, static_cast<int16_t>(ratio * (OLED_WIDTH - 2)), kBarHeight - 2, SSD1306_WHITE);

  _oled.display();
}
