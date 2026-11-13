#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class WifiStatus : uint8_t { Disconnected, Station, AccessPoint };

inline const char *wifiStatusText(WifiStatus status) {
  switch (status) {
    case WifiStatus::Station: return "OK";
    case WifiStatus::AccessPoint: return "AP";
    default: return "--";
  }
}

// Consistent copy of the runtime data shared between cores.
struct SystemSnapshot {
  float azReal = 0.0f;
  float azTarget = NAN; // NAN until a valid time allows the first solar calculation
  float elReal = 0.0f;
  float elTarget = NAN;
  float radiationV = 0.0f;
  WifiStatus wifi = WifiStatus::Disconnected;
  bool mqttConnected = false;
  char ip[16] = "0.0.0.0";

  // NAN targets are serialized as null.
  void writeTelemetry(JsonDocument &doc) const {
    doc["azimut_real"] = azReal;
    doc["azimut_obj"] = azTarget;
    doc["elevacion_real"] = elReal;
    doc["elevacion_obj"] = elTarget;
    doc["radiacion_v"] = radiationV;
  }
};

// Mutex-protected runtime state: Core 1 publishes measurements, Core 0 publishes connectivity.
class SystemState {
 public:
  void begin(SemaphoreHandle_t mutex) { _mutex = mutex; }

  void setMeasurements(float azReal, float elReal, float radiationV) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _data.azReal = azReal;
    _data.elReal = elReal;
    _data.radiationV = radiationV;
    xSemaphoreGive(_mutex);
  }

  void setTargets(float azTarget, float elTarget) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _data.azTarget = azTarget;
    _data.elTarget = elTarget;
    xSemaphoreGive(_mutex);
  }

  void setConnectivity(WifiStatus wifi, bool mqttConnected, const char *ip) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _data.wifi = wifi;
    _data.mqttConnected = mqttConnected;
    snprintf(_data.ip, sizeof(_data.ip), "%s", ip);
    xSemaphoreGive(_mutex);
  }

  SystemSnapshot get() const {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const SystemSnapshot copy = _data;
    xSemaphoreGive(_mutex);
    return copy;
  }

 private:
  SemaphoreHandle_t _mutex = nullptr;
  SystemSnapshot _data;
};
