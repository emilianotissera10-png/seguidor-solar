#pragma once

// ---------- Azimuth motor (ULN2003) ----------
#define PIN_AZ_IN1 19
#define PIN_AZ_IN2 18
#define PIN_AZ_IN3 5
#define PIN_AZ_IN4 17
#define AZ_MOTOR_INVERTED true

// ---------- Elevation motor (ULN2003) ----------
#define PIN_EL_IN1 16
#define PIN_EL_IN2 4
#define PIN_EL_IN3 2
#define PIN_EL_IN4 15
#define EL_MOTOR_INVERTED false

// ---------- Analog inputs (ADC1 only, ADC2 is unavailable while WiFi runs) ----------
#define PIN_POT_AZ 34
#define PIN_POT_EL 32
#define PIN_PANEL 35
#define ADC_OVERSAMPLES 16
#define V_REF 3.3
#define ADC_RES 4095.0

// ---------- Default ADC calibration (raw counts) ----------
#define ADC_AZ_MIN 2530 // Raw value at AZ_LIMIT_MIN_DEG
#define ADC_AZ_MAX 430  // Raw value at AZ_LIMIT_MAX_DEG
#define ADC_EL_0 3000   // Raw value at EL_LIMIT_MIN_DEG (horizontal)
#define ADC_EL_90 1500  // Raw value at EL_LIMIT_MAX_DEG (zenith)

// ---------- Mechanical limits (azimuth: 0 = North, + = East, - = West) ----------
#define AZ_LIMIT_MIN_DEG -70.0f
#define AZ_LIMIT_MAX_DEG 70.0f
#define EL_LIMIT_MIN_DEG 0.0f
#define EL_LIMIT_MAX_DEG 90.0f

// ---------- Position control ----------
#define DEADBAND_DEG 2.0f       // A motor starts moving when the error exceeds this value
#define STOP_BAND_DEG 1.0f      // A moving motor stops here, so ADC noise cannot retrigger it
#define MOTOR_STEP_PERIOD_MS 4
#define NIGHT_ELEVATION_DEG 0.0f

// ---------- Display (SSD1306 over I2C) ----------
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define OLED_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// ---------- Location and time ----------
#define DEFAULT_LATITUDE -32.95f
#define DEFAULT_LONGITUDE -60.66f
#define UTC_OFFSET_HOURS -3
#define NTP_SERVER "pool.ntp.org"
#define MIN_VALID_EPOCH 1700000000UL // System time below this value has never been set

// ---------- Default WiFi credentials ----------
#define DEFAULT_WIFI_SSID "MiRed"
#define DEFAULT_WIFI_PASS "MiPassword"

// ---------- Fallback access point and captive portal ----------
#define AP_SSID "ESP32_Solar_Config"
#define AP_PASSWORD "12345678"
#define DNS_PORT 53
#define HTTP_PORT 80

// ---------- Default MQTT broker ----------
#define DEFAULT_MQTT_HOST "192.168.1.100"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_TOPIC_PREFIX "utn_frr/seguidor_tissera_molinari"
// Suffixes appended to the configurable topic prefix
#define MQTT_SUFFIX_TELEMETRY "/telemetria"
#define MQTT_SUFFIX_COMMANDS "/comandos"
#define MQTT_SUFFIX_RESPONSES "/respuestas"
#define MQTT_BUFFER_SIZE 512

// ---------- Timing (ms) ----------
#define DEFAULT_TRACK_INTERVAL_MS 60000UL
#define TRACK_INTERVAL_MIN_MS 1000UL
#define TRACK_INTERVAL_MAX_MS 86400000UL
#define DEFAULT_TELEMETRY_INTERVAL_MS 5000UL
#define TELEMETRY_INTERVAL_MIN_MS 1000UL
#define TELEMETRY_INTERVAL_MAX_MS 3600000UL
#define CONTROL_PERIOD_MS 20
#define DISPLAY_PERIOD_MS 500UL
#define NETWORK_PERIOD_MS 5
#define WIFI_ATTEMPT_TIMEOUT_MS 10000UL
#define WIFI_MAX_ATTEMPTS 3
#define WIFI_AP_RETRY_MS 60000UL
#define MQTT_RETRY_MS 5000UL
#define MQTT_CONNECT_TIMEOUT_MS 1000UL

// ---------- Command processor ----------
#define SERIAL_BAUD 115200
#define CMD_MAX_LEN 160

// ---------- FreeRTOS tasks (Core 0: communications, Core 1: control) ----------
#define CORE_NETWORK 0
#define CORE_CONTROL 1
#define TASK_STACK_NETWORK 8192
#define TASK_STACK_CONTROL 4096
#define TASK_STACK_MOTOR 2048
#define TASK_PRIO_NETWORK 1
#define TASK_PRIO_CONTROL 2
#define TASK_PRIO_MOTOR 3
