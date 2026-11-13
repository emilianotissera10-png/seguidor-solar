#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>

#include "CommandProcessor.h"
#include "StorageManager.h"
#include "SystemState.h"

// WiFi FSM (station with access point fallback), NTP, MQTT and the HTTP portal.
// Named this way because the Arduino core already provides NetworkManager.
class ConnectivityManager {
 public:
  ConnectivityManager(StorageManager &storage, SystemState &state, CommandProcessor &commands);

  void begin();
  void update();

 private:
  enum class WifiState : uint8_t { Connecting, Connected, AccessPoint };

  void refreshSettings();
  void updateWifi();
  void updateMqtt();
  void buildTopics();
  void publishConnectivity();

  void startStationAttempt();
  void enterConnected();
  void startAccessPoint();

  void onMqttMessage(const char *topic, const uint8_t *payload, unsigned int length);
  void publishTelemetry();

  void setupWebRoutes();
  void handleRoot();
  void handleConfig();
  void handleStatus();
  void handleSave();
  void handleCommand();
  void handleNotFound();

  StorageManager &_storage;
  SystemState &_state;
  CommandProcessor &_commands;

  Settings _settings = {};
  uint32_t _settingsRevision = 0;

  WifiState _wifiState = WifiState::Connecting;
  uint8_t _wifiAttempts = 0;
  uint32_t _wifiAttemptStartMs = 0;

  WiFiClient _wifiClient;
  PubSubClient _mqtt;
  String _mqttClientId;
  // Prefix (up to 63 chars) plus the longest suffix fits in 80 bytes.
  char _topicTelemetry[80];
  char _topicCommands[80];
  char _topicResponses[80];
  uint32_t _lastMqttAttemptMs = 0;
  uint32_t _lastTelemetryMs = 0;

  WebServer _web;
  DNSServer _dns;

  WifiStatus _publishedWifi = WifiStatus::Disconnected;
  bool _publishedMqtt = false;
};
