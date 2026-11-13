#include "StorageManager.h"

#include <ctype.h>
#include <string.h>

#include "Config.h"

namespace {
const char *const kNamespace = "solar";
const char *const kKeySsid = "wifi_ssid";
const char *const kKeyPass = "wifi_pass";
const char *const kKeyHost = "mqtt_host";
const char *const kKeyPort = "mqtt_port";
const char *const kKeyTopic = "mqtt_topic";
const char *const kKeyLat = "lat";
const char *const kKeyLon = "lon";
const char *const kKeyInterval = "interval";
const char *const kKeyTelemetry = "telemetry";
const char *const kKeyAzMin = "cal_az_min";
const char *const kKeyAzMax = "cal_az_max";
const char *const kKeyEl0 = "cal_el_0";
const char *const kKeyEl90 = "cal_el_90";

// Accepts IPv4 addresses and DNS names: letters, digits, '-' and '.'.
bool isValidHost(const char *host) {
  if (*host == '\0') return false;
  for (; *host != '\0'; host++) {
    if (!isalnum(static_cast<unsigned char>(*host)) && *host != '-' && *host != '.') return false;
  }
  return true;
}

// Rejects MQTT wildcards, whitespace and leading or trailing '/'.
bool isValidTopicPrefix(const char *prefix) {
  const size_t length = strlen(prefix);
  if (length == 0 || prefix[0] == '/' || prefix[length - 1] == '/') return false;
  for (size_t i = 0; i < length; i++) {
    if (prefix[i] == '+' || prefix[i] == '#' || isspace(static_cast<unsigned char>(prefix[i]))) return false;
  }
  return true;
}
}  // namespace

bool StorageManager::begin(SemaphoreHandle_t mutex) {
  _mutex = mutex;
  const bool opened = _prefs.begin(kNamespace, false);

  // Getters return the macro defaults for keys missing from NVS.
  _prefs.getString(kKeySsid, _settings.wifiSsid, sizeof(_settings.wifiSsid));
  _prefs.getString(kKeyPass, _settings.wifiPass, sizeof(_settings.wifiPass));
  _prefs.getString(kKeyHost, _settings.mqttHost, sizeof(_settings.mqttHost));
  if (!_prefs.isKey(kKeySsid)) {
    snprintf(_settings.wifiSsid, sizeof(_settings.wifiSsid), "%s", DEFAULT_WIFI_SSID);
    snprintf(_settings.wifiPass, sizeof(_settings.wifiPass), "%s", DEFAULT_WIFI_PASS);
  }
  if (!_prefs.isKey(kKeyHost)) {
    snprintf(_settings.mqttHost, sizeof(_settings.mqttHost), "%s", DEFAULT_MQTT_HOST);
  }
  if (!_prefs.isKey(kKeyTopic) ||
      _prefs.getString(kKeyTopic, _settings.mqttTopic, sizeof(_settings.mqttTopic)) == 0) {
    snprintf(_settings.mqttTopic, sizeof(_settings.mqttTopic), "%s", DEFAULT_MQTT_TOPIC_PREFIX);
  }
  _settings.mqttPort = _prefs.getUShort(kKeyPort, DEFAULT_MQTT_PORT);
  _settings.latitude = _prefs.getFloat(kKeyLat, DEFAULT_LATITUDE);
  _settings.longitude = _prefs.getFloat(kKeyLon, DEFAULT_LONGITUDE);
  _settings.trackIntervalMs = _prefs.getULong(kKeyInterval, DEFAULT_TRACK_INTERVAL_MS);
  _settings.telemetryIntervalMs = _prefs.getULong(kKeyTelemetry, DEFAULT_TELEMETRY_INTERVAL_MS);
  _settings.adcAzMin = _prefs.getUShort(kKeyAzMin, ADC_AZ_MIN);
  _settings.adcAzMax = _prefs.getUShort(kKeyAzMax, ADC_AZ_MAX);
  _settings.adcEl0 = _prefs.getUShort(kKeyEl0, ADC_EL_0);
  _settings.adcEl90 = _prefs.getUShort(kKeyEl90, ADC_EL_90);
  return opened;
}

Settings StorageManager::get() const {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const Settings copy = _settings;
  xSemaphoreGive(_mutex);
  return copy;
}

bool StorageManager::setWifi(const char *ssid, const char *pass) {
  const size_t ssidLen = strlen(ssid);
  const size_t passLen = strlen(pass);
  // An empty password selects an open network; WPA2 requires 8 to 63 characters.
  if (ssidLen == 0 || ssidLen >= sizeof(_settings.wifiSsid)) return false;
  if (passLen >= sizeof(_settings.wifiPass) || (passLen > 0 && passLen < 8)) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (strcmp(ssid, _settings.wifiSsid) != 0 || strcmp(pass, _settings.wifiPass) != 0) {
    ok = _prefs.putString(kKeyPass, pass) == passLen && _prefs.putString(kKeySsid, ssid) == ssidLen;
    if (ok) {
      memcpy(_settings.wifiSsid, ssid, ssidLen + 1);
      memcpy(_settings.wifiPass, pass, passLen + 1);
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}

bool StorageManager::setMqtt(const char *host, uint16_t port) {
  if (port == 0 || strlen(host) >= sizeof(_settings.mqttHost) || !isValidHost(host)) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (strcmp(host, _settings.mqttHost) != 0 || port != _settings.mqttPort) {
    ok = _prefs.putString(kKeyHost, host) == strlen(host) && _prefs.putUShort(kKeyPort, port) > 0;
    if (ok) {
      snprintf(_settings.mqttHost, sizeof(_settings.mqttHost), "%s", host);
      _settings.mqttPort = port;
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}

bool StorageManager::setMqttTopic(const char *prefix) {
  const size_t length = strlen(prefix);
  if (length >= sizeof(_settings.mqttTopic) || !isValidTopicPrefix(prefix)) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (strcmp(prefix, _settings.mqttTopic) != 0) {
    ok = _prefs.putString(kKeyTopic, prefix) == length;
    if (ok) {
      memcpy(_settings.mqttTopic, prefix, length + 1);
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}

bool StorageManager::setLocation(float latitude, float longitude) {
  // Written so that NAN inputs are rejected as well.
  if (!(latitude >= -90.0f && latitude <= 90.0f)) return false;
  if (!(longitude >= -180.0f && longitude <= 180.0f)) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (latitude != _settings.latitude || longitude != _settings.longitude) {
    ok = _prefs.putFloat(kKeyLat, latitude) > 0 && _prefs.putFloat(kKeyLon, longitude) > 0;
    if (ok) {
      _settings.latitude = latitude;
      _settings.longitude = longitude;
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}

bool StorageManager::setTrackInterval(uint32_t intervalMs) {
  if (intervalMs < TRACK_INTERVAL_MIN_MS || intervalMs > TRACK_INTERVAL_MAX_MS) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (intervalMs != _settings.trackIntervalMs) {
    ok = _prefs.putULong(kKeyInterval, intervalMs) > 0;
    if (ok) {
      _settings.trackIntervalMs = intervalMs;
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}

bool StorageManager::setTelemetryInterval(uint32_t intervalMs) {
  if (intervalMs < TELEMETRY_INTERVAL_MIN_MS || intervalMs > TELEMETRY_INTERVAL_MAX_MS) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (intervalMs != _settings.telemetryIntervalMs) {
    ok = _prefs.putULong(kKeyTelemetry, intervalMs) > 0;
    if (ok) {
      _settings.telemetryIntervalMs = intervalMs;
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}

bool StorageManager::setCalibration(uint16_t azMin, uint16_t azMax, uint16_t el0, uint16_t el90) {
  // Equal end points would cause a division by zero in the interpolation.
  if (azMin == azMax || el0 == el90) return false;
  if (azMin > ADC_RES || azMax > ADC_RES || el0 > ADC_RES || el90 > ADC_RES) return false;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = true;
  if (azMin != _settings.adcAzMin || azMax != _settings.adcAzMax || el0 != _settings.adcEl0 ||
      el90 != _settings.adcEl90) {
    ok = _prefs.putUShort(kKeyAzMin, azMin) > 0 && _prefs.putUShort(kKeyAzMax, azMax) > 0 &&
         _prefs.putUShort(kKeyEl0, el0) > 0 && _prefs.putUShort(kKeyEl90, el90) > 0;
    if (ok) {
      _settings.adcAzMin = azMin;
      _settings.adcAzMax = azMax;
      _settings.adcEl0 = el0;
      _settings.adcEl90 = el90;
      _revision++;
    }
  }
  xSemaphoreGive(_mutex);
  return ok;
}
