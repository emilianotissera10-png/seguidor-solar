#include "CommandProcessor.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "Config.h"

namespace {
const uint8_t kMaxTokens = 6;  // Verb, key and up to four arguments
const char *const kOk = "OK";
const char *const kInvalid = "ERR: invalid value or NVS write failed";

// Splits in place, honoring double quotes. Returns -1 on malformed input.
int tokenize(char *line, char **tokens) {
  int count = 0;
  char *cursor = line;
  while (true) {
    while (isspace(static_cast<unsigned char>(*cursor))) cursor++;
    if (*cursor == '\0') return count;
    if (count == kMaxTokens) return -1;

    if (*cursor == '"') {
      tokens[count++] = ++cursor;
      cursor = strchr(cursor, '"');
      if (cursor == nullptr) return -1;
    } else {
      tokens[count++] = cursor;
      while (*cursor != '\0' && !isspace(static_cast<unsigned char>(*cursor))) cursor++;
      if (*cursor == '\0') return count;
    }
    *cursor++ = '\0';
  }
}

bool parseFloat(const char *text, float &value) {
  char *end = nullptr;
  value = strtof(text, &end);
  return end != text && *end == '\0';
}

bool parseUnsigned(const char *text, uint32_t maxValue, uint32_t &value) {
  if (!isdigit(static_cast<unsigned char>(*text))) return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  value = parsed;
  return *end == '\0' && parsed <= maxValue;
}
}  // namespace

const CommandProcessor::Command CommandProcessor::kCommands[] = {
    {"get", "status", 0, 0, &CommandProcessor::getStatus},
    {"get", "wifi", 0, 0, &CommandProcessor::getWifi},
    {"set", "wifi", 1, 2, &CommandProcessor::setWifi},
    {"get", "mqtt", 0, 0, &CommandProcessor::getMqtt},
    {"set", "mqtt", 2, 2, &CommandProcessor::setMqtt},
    {"get", "topic", 0, 0, &CommandProcessor::getTopic},
    {"set", "topic", 1, 1, &CommandProcessor::setTopic},
    {"get", "location", 0, 0, &CommandProcessor::getLocation},
    {"set", "location", 2, 2, &CommandProcessor::setLocation},
    {"get", "interval", 0, 0, &CommandProcessor::getInterval},
    {"set", "interval", 1, 1, &CommandProcessor::setInterval},
    {"get", "telemetry", 0, 0, &CommandProcessor::getTelemetry},
    {"set", "telemetry", 1, 1, &CommandProcessor::setTelemetry},
    {"get", "cal", 0, 0, &CommandProcessor::getCal},
    {"set", "cal", 4, 4, &CommandProcessor::setCal},
};

CommandProcessor::CommandProcessor(StorageManager &storage, SystemState &state)
    : _storage(storage), _state(state) {}

String CommandProcessor::process(const char *input) {
  while (isspace(static_cast<unsigned char>(*input))) input++;

  char line[CMD_MAX_LEN];
  if (*input == '{') {
    JsonDocument doc;
    if (deserializeJson(doc, input)) return "ERR: invalid JSON";
    const char *command = doc["cmd"];
    if (command == nullptr) return "ERR: missing cmd field";
    if (strlen(command) >= sizeof(line)) return "ERR: command too long";
    strcpy(line, command);
  } else {
    if (strlen(input) >= sizeof(line)) return "ERR: command too long";
    strcpy(line, input);
  }

  char *tokens[kMaxTokens];
  const int count = tokenize(line, tokens);
  if (count < 0) return "ERR: malformed command";
  if (count < 2) return "ERR: unknown command";

  const uint8_t argCount = count - 2;
  for (const Command &command : kCommands) {
    if (strcmp(tokens[0], command.verb) == 0 && strcmp(tokens[1], command.key) == 0) {
      if (argCount < command.minArgs || argCount > command.maxArgs) return "ERR: wrong number of arguments";
      return (this->*command.handler)(&tokens[2], argCount);
    }
  }
  return "ERR: unknown command";
}

String CommandProcessor::getStatus(char *const *, uint8_t) {
  const SystemSnapshot snapshot = _state.get();
  JsonDocument doc;
  snapshot.writeTelemetry(doc);
  doc["ip"] = snapshot.ip;
  doc["wifi"] = wifiStatusText(snapshot.wifi);
  doc["mqtt"] = snapshot.mqttConnected ? "OK" : "--";
  String response;
  serializeJson(doc, response);
  return response;
}

String CommandProcessor::getWifi(char *const *, uint8_t) {
  const Settings settings = _storage.get();
  // The password is masked on every channel.
  return String("ssid=") + settings.wifiSsid + " pass=********";
}

String CommandProcessor::setWifi(char *const *args, uint8_t argCount) {
  return _storage.setWifi(args[0], argCount == 2 ? args[1] : "") ? kOk : kInvalid;
}

String CommandProcessor::getMqtt(char *const *, uint8_t) {
  const Settings settings = _storage.get();
  return String("host=") + settings.mqttHost + " port=" + settings.mqttPort;
}

String CommandProcessor::getTopic(char *const *, uint8_t) {
  return String("topic=") + _storage.get().mqttTopic;
}

String CommandProcessor::setTopic(char *const *args, uint8_t) {
  return _storage.setMqttTopic(args[0]) ? kOk : kInvalid;
}

String CommandProcessor::setMqtt(char *const *args, uint8_t) {
  uint32_t port = 0;
  if (!parseUnsigned(args[1], UINT16_MAX, port)) return kInvalid;
  return _storage.setMqtt(args[0], port) ? kOk : kInvalid;
}

String CommandProcessor::getLocation(char *const *, uint8_t) {
  const Settings settings = _storage.get();
  return String("lat=") + String(settings.latitude, 4) + " lon=" + String(settings.longitude, 4);
}

String CommandProcessor::setLocation(char *const *args, uint8_t) {
  float latitude = 0.0f;
  float longitude = 0.0f;
  if (!parseFloat(args[0], latitude) || !parseFloat(args[1], longitude)) return kInvalid;
  return _storage.setLocation(latitude, longitude) ? kOk : kInvalid;
}

String CommandProcessor::getInterval(char *const *, uint8_t) {
  return String("interval=") + _storage.get().trackIntervalMs;
}

String CommandProcessor::setInterval(char *const *args, uint8_t) {
  uint32_t interval = 0;
  if (!parseUnsigned(args[0], TRACK_INTERVAL_MAX_MS, interval)) return kInvalid;
  return _storage.setTrackInterval(interval) ? kOk : kInvalid;
}

String CommandProcessor::getTelemetry(char *const *, uint8_t) {
  return String("telemetry=") + _storage.get().telemetryIntervalMs;
}

String CommandProcessor::setTelemetry(char *const *args, uint8_t) {
  uint32_t interval = 0;
  if (!parseUnsigned(args[0], TELEMETRY_INTERVAL_MAX_MS, interval)) return kInvalid;
  return _storage.setTelemetryInterval(interval) ? kOk : kInvalid;
}

String CommandProcessor::getCal(char *const *, uint8_t) {
  const Settings settings = _storage.get();
  return String("az_min=") + settings.adcAzMin + " az_max=" + settings.adcAzMax + " el_min=" + settings.adcEl0 +
         " el_max=" + settings.adcEl90;
}

String CommandProcessor::setCal(char *const *args, uint8_t) {
  uint32_t values[4];
  for (uint8_t i = 0; i < 4; i++) {
    if (!parseUnsigned(args[i], static_cast<uint32_t>(ADC_RES), values[i])) return kInvalid;
  }
  return _storage.setCalibration(values[0], values[1], values[2], values[3]) ? kOk : kInvalid;
}
