#pragma once

#include <Arduino.h>

// Oversampled ADC1 acquisition. Used only from the control task.
class SensorManager {
 public:
  void begin();
  void setCalibration(uint16_t azMin, uint16_t azMax, uint16_t el0, uint16_t el90);
  void update();

  float azimuthDeg() const { return _azimuthDeg; }
  float elevationDeg() const { return _elevationDeg; }
  float panelVolts() const { return _panelVolts; }

 private:
  static float readAveraged(uint8_t pin);
  static float interpolate(float raw, float raw0, float raw1, float deg0, float deg1);

  uint16_t _azMin = 0;
  uint16_t _azMax = 0;
  uint16_t _el0 = 0;
  uint16_t _el90 = 0;
  float _azimuthDeg = 0.0f;
  float _elevationDeg = 0.0f;
  float _panelVolts = 0.0f;
};
