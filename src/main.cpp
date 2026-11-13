#include <Arduino.h>

#include "CommandProcessor.h"
#include "Config.h"
#include "ConnectivityManager.h"
#include "DisplayManager.h"
#include "MotorController.h"
#include "SensorManager.h"
#include "SolarCalculatorModule.h"
#include "StorageManager.h"
#include "SystemState.h"

namespace {
enum class TrackState : uint8_t { WaitTime, Tracking, Night };

StorageManager storage;
SystemState systemState;
SensorManager sensors;
SolarCalculatorModule solar;
DisplayManager display;
CommandProcessor commands(storage, systemState);
ConnectivityManager connectivity(storage, systemState, commands);
MotorController azMotor(PIN_AZ_IN1, PIN_AZ_IN2, PIN_AZ_IN3, PIN_AZ_IN4, AZ_MOTOR_INVERTED);
MotorController elMotor(PIN_EL_IN1, PIN_EL_IN2, PIN_EL_IN3, PIN_EL_IN4, EL_MOTOR_INVERTED);

// Non-blocking serial transport for the command processor.
void pollSerial() {
  static char line[CMD_MAX_LEN];
  static size_t length = 0;
  static bool overflow = false;

  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      if (overflow) {
        Serial.println(F("ERR: command too long"));
      } else if (length > 0) {
        line[length] = '\0';
        Serial.println(commands.process(line));
      }
      length = 0;
      overflow = false;
    } else if (length < sizeof(line) - 1) {
      line[length++] = c;
    } else {
      overflow = true;
    }
  }
}

void Core0_NetworkTask(void *) {
  connectivity.begin();
  for (;;) {
    pollSerial();
    connectivity.update();
    vTaskDelay(pdMS_TO_TICKS(NETWORK_PERIOD_MS));
  }
}

// Highest priority on Core 1, so I2C transfers cannot disturb the step timing.
void Core1_MotorTask(void *) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    azMotor.tick();
    elMotor.tick();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(MOTOR_STEP_PERIOD_MS));
  }
}

void Core1_ControlTask(void *) {
  TrackState trackState = TrackState::WaitTime;
  Settings settings = {};
  uint32_t appliedRevision = storage.revision() - 1;  // Forces the first load
  bool solarDue = true;
  uint32_t lastSolarMs = 0;
  uint32_t lastDisplayMs = 0;
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    const uint32_t nowMs = millis();

    const uint32_t revision = storage.revision();
    if (revision != appliedRevision) {
      appliedRevision = revision;
      settings = storage.get();
      sensors.setCalibration(settings.adcAzMin, settings.adcAzMax, settings.adcEl0, settings.adcEl90);
      solar.setLocation(settings.latitude, settings.longitude);
      solarDue = true;
    }

    sensors.update();
    systemState.setMeasurements(sensors.azimuthDeg(), sensors.elevationDeg(), sensors.panelVolts());

    if (solarDue || nowMs - lastSolarMs >= settings.trackIntervalMs) {
      solarDue = false;
      lastSolarMs = nowMs;
      if (solar.update()) {
        trackState = solar.isNight() ? TrackState::Night : TrackState::Tracking;
        systemState.setTargets(solar.targetAzimuth(), solar.targetElevation());
      }
    }

    switch (trackState) {
      case TrackState::WaitTime:
        // Motors stay still until NTP or the web portal provides the time.
        solarDue = SolarCalculatorModule::hasValidTime();
        azMotor.stop();
        elMotor.stop();
        break;
      case TrackState::Tracking:
        azMotor.update(sensors.azimuthDeg(), solar.targetAzimuth());
        elMotor.update(sensors.elevationDeg(), solar.targetElevation());
        break;
      case TrackState::Night:
        azMotor.stop();
        elMotor.stop();
        break;
    }

    if (nowMs - lastDisplayMs >= DISPLAY_PERIOD_MS) {
      lastDisplayMs = nowMs;
      display.update(systemState.get(), settings.latitude, settings.longitude);
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
  }
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);

  SemaphoreHandle_t storageMutex = xSemaphoreCreateMutex();
  SemaphoreHandle_t stateMutex = xSemaphoreCreateMutex();
  configASSERT(storageMutex);
  configASSERT(stateMutex);

  systemState.begin(stateMutex);
  if (!storage.begin(storageMutex)) Serial.println(F("[NVS] Open failed, using defaults"));

  // Coils are released before anything else can run.
  azMotor.begin();
  elMotor.begin();
  sensors.begin();
  if (!display.begin()) Serial.println(F("[OLED] Not found"));

  xTaskCreatePinnedToCore(Core0_NetworkTask, "Core0_NetworkTask", TASK_STACK_NETWORK, nullptr, TASK_PRIO_NETWORK,
                          nullptr, CORE_NETWORK);
  xTaskCreatePinnedToCore(Core1_MotorTask, "Core1_MotorTask", TASK_STACK_MOTOR, nullptr, TASK_PRIO_MOTOR, nullptr,
                          CORE_CONTROL);
  xTaskCreatePinnedToCore(Core1_ControlTask, "Core1_ControlTask", TASK_STACK_CONTROL, nullptr, TASK_PRIO_CONTROL,
                          nullptr, CORE_CONTROL);
}

void loop() {
  // All work runs in the pinned tasks.
  vTaskDelete(nullptr);
}
