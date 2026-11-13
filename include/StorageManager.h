#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct Settings {
  char wifiSsid[33];
  char wifiPass[64];
  char mqttHost[64];  // IPv4 address or DNS name
  uint16_t mqttPort;
  char mqttTopic[64];  // Topic prefix, without a trailing '/'
  float latitude;
  float longitude;
  uint32_t trackIntervalMs;
  uint32_t telemetryIntervalMs;
  uint16_t adcAzMin;
  uint16_t adcAzMax;
  uint16_t adcEl0;
  uint16_t adcEl90;
};

// Thread-safe NVS wrapper with a RAM cache. Setters validate their input and
// return false if the value is invalid or could not be persisted.
class StorageManager {
 public:
  bool begin(SemaphoreHandle_t mutex);

  Settings get() const;

  // Incremented on every change so tasks can detect when to refresh their copy.
  uint32_t revision() const { return _revision.load(); }

  bool setWifi(const char *ssid, const char *pass);
  bool setMqtt(const char *host, uint16_t port);
  bool setMqttTopic(const char *prefix);
  bool setLocation(float latitude, float longitude);
  bool setTrackInterval(uint32_t intervalMs);
  bool setTelemetryInterval(uint32_t intervalMs);
  bool setCalibration(uint16_t azMin, uint16_t azMax, uint16_t el0, uint16_t el90);

 private:
  Preferences _prefs;
  SemaphoreHandle_t _mutex = nullptr;
  Settings _settings = {};
  std::atomic<uint32_t> _revision{0};
};
