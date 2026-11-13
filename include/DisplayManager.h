#pragma once

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "SystemState.h"

// SSD1306 status screen. Used only from the control task.
class DisplayManager {
 public:
  DisplayManager();

  // Returns false if the display does not answer on the I2C bus.
  bool begin();
  void update(const SystemSnapshot &state, float latitude, float longitude);

 private:
  Adafruit_SSD1306 _oled;
  bool _available = false;
};
