#pragma once

#include <Arduino.h>

#include "StorageManager.h"
#include "SystemState.h"

// Transport-independent get/set processor. Every transport runs in the network
// task, so calls are never concurrent.
//
// Accepted input:
//   Plain text: set location -32.95 -60.66
//   JSON:       {"cmd":"set location -32.95 -60.66"}
// Arguments containing spaces are written between double quotes.
class CommandProcessor {
 public:
  CommandProcessor(StorageManager &storage, SystemState &state);

  String process(const char *input);

 private:
  using Handler = String (CommandProcessor::*)(char *const *args, uint8_t argCount);

  // New commands only need a row in kCommands and their handler.
  struct Command {
    const char *verb;
    const char *key;
    uint8_t minArgs;
    uint8_t maxArgs;
    Handler handler;
  };
  static const Command kCommands[];

  String getStatus(char *const *args, uint8_t argCount);
  String getWifi(char *const *args, uint8_t argCount);
  String setWifi(char *const *args, uint8_t argCount);
  String getMqtt(char *const *args, uint8_t argCount);
  String setMqtt(char *const *args, uint8_t argCount);
  String getTopic(char *const *args, uint8_t argCount);
  String setTopic(char *const *args, uint8_t argCount);
  String getLocation(char *const *args, uint8_t argCount);
  String setLocation(char *const *args, uint8_t argCount);
  String getInterval(char *const *args, uint8_t argCount);
  String setInterval(char *const *args, uint8_t argCount);
  String getTelemetry(char *const *args, uint8_t argCount);
  String setTelemetry(char *const *args, uint8_t argCount);
  String getCal(char *const *args, uint8_t argCount);
  String setCal(char *const *args, uint8_t argCount);

  StorageManager &_storage;
  SystemState &_state;
};
