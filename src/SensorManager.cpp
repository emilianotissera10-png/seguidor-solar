#include "SensorManager.h"

#include "Config.h"

void SensorManager::begin() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void SensorManager::setCalibration(uint16_t azMin, uint16_t azMax, uint16_t el0, uint16_t el90) {
  _azMin = azMin;
  _azMax = azMax;
  _el0 = el0;
  _el90 = el90;
}

void SensorManager::update() {
  _azimuthDeg = interpolate(readAveraged(PIN_POT_AZ), _azMin, _azMax, AZ_LIMIT_MIN_DEG, AZ_LIMIT_MAX_DEG);
  _elevationDeg = interpolate(readAveraged(PIN_POT_EL), _el0, _el90, EL_LIMIT_MIN_DEG, EL_LIMIT_MAX_DEG);
  _panelVolts = readAveraged(PIN_PANEL) * V_REF / ADC_RES;
}

float SensorManager::readAveraged(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < ADC_OVERSAMPLES; i++) {
    sum += analogRead(pin);
  }
  return static_cast<float>(sum) / ADC_OVERSAMPLES;
}

float SensorManager::interpolate(float raw, float raw0, float raw1, float deg0, float deg1) {
  return deg0 + (raw - raw0) * (deg1 - deg0) / (raw1 - raw0);
}
