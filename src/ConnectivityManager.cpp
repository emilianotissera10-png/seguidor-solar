#include "ConnectivityManager.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "Config.h"

namespace {
// Static single-page app: a hamburger menu switches between sections, the form is
// filled from /config and the status section polls /status.
const char kPage[] PROGMEM = R"rawhtml(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content='width=device-width,initial-scale=1'><title>Seguidor Solar</title>
<style>
body{font-family:sans-serif;margin:0;background:#f2f4f7;color:#222}
header{display:flex;align-items:center;gap:12px;background:#1e3a5f;color:#fff;padding:10px 16px}
header h1{font-size:20px;margin:0}
#mb{background:none;border:0;color:#fff;font-size:26px;cursor:pointer}
nav{display:none;background:#2b4f7a}nav.open{display:block}
nav a{display:block;color:#fff;padding:12px 16px;text-decoration:none;cursor:pointer}
nav a:hover{background:#3a6699}
main{max-width:440px;margin:auto;padding:16px}
section{display:none}section.on{display:block}
label{display:block;margin-top:12px}
input{width:100%;box-sizing:border-box;padding:8px;margin-top:4px}
button.s{margin-top:16px;width:100%;padding:12px;background:#1e3a5f;color:#fff;border:0;border-radius:4px}
table{width:100%;border-collapse:collapse;background:#fff}
td{padding:10px;border-bottom:1px solid #ddd}td+td{text-align:right;font-weight:bold}
.ok{color:#18803c}.bad{color:#c62828}
</style></head><body>
<header><button id=mb onclick="nv.classList.toggle('open')">&#9776;</button><h1>Seguidor Solar</h1></header>
<nav id=nv><a onclick="show('cfg')">Configuración</a><a onclick="show('st')">Estado</a><a onclick="show('ab')">Acerca de</a></nav>
<main>
<section id=cfg class=on><h2>Configuración</h2><form id=f>
<label>Red WiFi (SSID)<input name=ssid maxlength=32></label>
<label>Contraseña WiFi (vacía = mantener la actual)<input name=pass type=password maxlength=63></label>
<label>Broker MQTT (IP o dominio)<input name=broker maxlength=63></label>
<label>Puerto MQTT<input name=port type=number></label>
<label>Prefijo de tópicos MQTT<input name=topic maxlength=63></label>
<label>Latitud<input name=lat type=number step=any></label>
<label>Longitud<input name=lon type=number step=any></label>
<label>Intervalo de telemetría (ms)<input name=tel type=number></label>
<input type=hidden name=ep><button class=s>Guardar y sincronizar hora</button></form>
<p id=msg></p></section>
<section id=st><h2>Estado</h2><table>
<tr><td>WiFi<td id=wifi>--
<tr><td>MQTT<td id=mqtt>--
<tr><td>Azimut (real / objetivo)<td id=az>--
<tr><td>Elevación (real / objetivo)<td id=el>--
<tr><td>Panel solar<td id=rad>--
</table></section>
<section id=ab><h2>Acerca de</h2>
<p>Seguidor solar bi-axial de lazo cerrado basado en ESP32 y FreeRTOS. Calcula la posición del sol a partir de la hora
y la ubicación, y orienta el panel con dos motores paso a paso realimentados por potenciómetros. Publica su telemetría
por MQTT y se configura por Serial, MQTT o esta página.</p>
<p>Cátedra Electrónica II, UTN FRRO.</p>
<p><b>Autores:</b> Damián Molinari y Emiliano Tissera</p></section>
</main><script>
var f=document.getElementById('f'),timer;
function $(id){return document.getElementById(id)}
function show(id){
document.querySelectorAll('section').forEach(function(s){s.classList.toggle('on',s.id==id)});
nv.classList.remove('open');clearInterval(timer);
if(id=='st'){poll();timer=setInterval(poll,2000)}}
function deg(v){return v==null?'--':v.toFixed(1)+'°'}
function flag(id,ok,text){$(id).textContent=text;$(id).className=ok?'ok':'bad'}
function poll(){fetch('/status').then(function(r){return r.json()}).then(function(d){
flag('wifi',d.wifi!='--',d.wifi=='OK'?'Conectado ('+d.ip+')':d.wifi=='AP'?'Punto de acceso ('+d.ip+')':'Desconectado');
flag('mqtt',d.mqtt=='OK',d.mqtt=='OK'?'Conectado':'Desconectado');
$('az').textContent=deg(d.azimut_real)+' / '+deg(d.azimut_obj);
$('el').textContent=deg(d.elevacion_real)+' / '+deg(d.elevacion_obj);
$('rad').textContent=d.radiacion_v.toFixed(2)+' V'}).catch(function(){flag('wifi',false,'Sin respuesta')})}
fetch('/config').then(function(r){return r.json()}).then(function(c){for(var k in c)f.elements[k].value=c[k]});
f.onsubmit=function(e){e.preventDefault();f.ep.value=Math.floor(Date.now()/1000);
fetch('/save',{method:'POST',body:new URLSearchParams(new FormData(f))}).then(function(r){return r.text()})
.then(function(t){$('msg').textContent=t;f.pass.value=''}).catch(function(){$('msg').textContent='Sin respuesta'})}
</script></body></html>)rawhtml";

bool parseFloatArg(const String &text, float &value) {
  char *end = nullptr;
  value = strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0';
}

bool parseUnsignedArg(const String &text, uint32_t &value) {
  if (!isdigit(static_cast<unsigned char>(text[0]))) return false;
  char *end = nullptr;
  value = strtoul(text.c_str(), &end, 10);
  return *end == '\0';
}
}  // namespace

ConnectivityManager::ConnectivityManager(StorageManager &storage, SystemState &state, CommandProcessor &commands)
    : _storage(storage), _state(state), _commands(commands), _mqtt(_wifiClient), _web(HTTP_PORT) {}

void ConnectivityManager::begin() {
  _settings = _storage.get();
  _settingsRevision = _storage.revision();

  // The FSM owns reconnection, and credentials are stored only by StorageManager.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);

  // Keeps an unreachable broker from stalling this task for the default timeout.
  _wifiClient.setConnectionTimeout(MQTT_CONNECT_TIMEOUT_MS);

  // The full 48-bit MAC keeps the ID unique: its low 32 bits alone include the vendor prefix shared by every ESP32.
  const uint64_t mac = ESP.getEfuseMac();
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "ESP32_Solar_%04X%08lX", static_cast<unsigned>(mac >> 32),
           static_cast<unsigned long>(mac & 0xFFFFFFFFUL));
  _mqttClientId = clientId;
  buildTopics();
  // A DNS name is resolved by WiFiClient on every connection attempt.
  _mqtt.setServer(_settings.mqttHost, _settings.mqttPort);
  _mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  _mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    onMqttMessage(topic, payload, length);
  });

  setupWebRoutes();
  _web.begin();

  startStationAttempt();
}

void ConnectivityManager::update() {
  refreshSettings();
  updateWifi();
  _web.handleClient();
  updateMqtt();
  publishConnectivity();
}

void ConnectivityManager::refreshSettings() {
  const uint32_t revision = _storage.revision();
  if (revision == _settingsRevision) return;
  _settingsRevision = revision;

  const Settings previous = _settings;
  _settings = _storage.get();

  // A new topic prefix also reconnects, so the subscription moves to the new command topic.
  if (strcmp(previous.mqttHost, _settings.mqttHost) != 0 || previous.mqttPort != _settings.mqttPort ||
      strcmp(previous.mqttTopic, _settings.mqttTopic) != 0) {
    _mqtt.disconnect();
    buildTopics();
    _mqtt.setServer(_settings.mqttHost, _settings.mqttPort);
    _lastMqttAttemptMs = millis() - MQTT_RETRY_MS;
  }

  if (strcmp(previous.wifiSsid, _settings.wifiSsid) != 0 || strcmp(previous.wifiPass, _settings.wifiPass) != 0) {
    _mqtt.disconnect();
    _wifiAttempts = 0;
    // The access point stays up during the attempt, so a failure keeps the portal reachable.
    if (_wifiState == WifiState::Connected) _wifiState = WifiState::Connecting;
    startStationAttempt();
  }
}

void ConnectivityManager::updateWifi() {
  const bool linked = WiFi.status() == WL_CONNECTED;
  const uint32_t elapsed = millis() - _wifiAttemptStartMs;

  switch (_wifiState) {
    case WifiState::Connecting:
      if (linked) {
        enterConnected();
      } else if (elapsed >= WIFI_ATTEMPT_TIMEOUT_MS) {
        if (++_wifiAttempts >= WIFI_MAX_ATTEMPTS) {
          startAccessPoint();
        } else {
          startStationAttempt();
        }
      }
      break;

    case WifiState::Connected:
      if (!linked) {
        Serial.println(F("[WiFi] Link lost"));
        _mqtt.disconnect();
        _wifiAttempts = 0;
        _wifiState = WifiState::Connecting;
        startStationAttempt();
      }
      break;

    case WifiState::AccessPoint:
      _dns.processNextRequest();
      if (linked) {
        _dns.stop();
        WiFi.softAPdisconnect(true);
        enterConnected();
      } else if (elapsed >= WIFI_AP_RETRY_MS && WiFi.softAPgetStationNum() == 0) {
        // Station attempts hop channels, so they wait until no client is using the portal.
        startStationAttempt();
      }
      break;
  }
}

void ConnectivityManager::startStationAttempt() {
  WiFi.disconnect();
  WiFi.begin(_settings.wifiSsid, _settings.wifiPass);
  _wifiAttemptStartMs = millis();
}

void ConnectivityManager::enterConnected() {
  _wifiState = WifiState::Connected;
  _wifiAttempts = 0;
  // Connect to the broker without waiting for the first retry period.
  _lastMqttAttemptMs = millis() - MQTT_RETRY_MS;
  configTime(0, 0, NTP_SERVER);
  Serial.printf("[WiFi] Connected, IP %s\n", WiFi.localIP().toString().c_str());
}

void ConnectivityManager::startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  _dns.start(DNS_PORT, "*", WiFi.softAPIP());
  _wifiState = WifiState::AccessPoint;
  _wifiAttempts = 0;
  _wifiAttemptStartMs = millis();
  Serial.printf("[WiFi] Access point %s, IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void ConnectivityManager::updateMqtt() {
  if (_wifiState != WifiState::Connected) return;

  if (!_mqtt.connected()) {
    if (millis() - _lastMqttAttemptMs < MQTT_RETRY_MS) return;
    _lastMqttAttemptMs = millis();
    if (_mqtt.connect(_mqttClientId.c_str())) {
      _mqtt.subscribe(_topicCommands);
      Serial.println(F("[MQTT] Connected"));
    }
    return;
  }

  _mqtt.loop();
  // The interval comes from NVS, so a new value applies on the next cycle.
  if (millis() - _lastTelemetryMs >= _settings.telemetryIntervalMs) {
    _lastTelemetryMs = millis();
    publishTelemetry();
  }
}

void ConnectivityManager::onMqttMessage(const char *, const uint8_t *payload, unsigned int length) {
  // PubSubClient reuses its buffer for publishing, so the payload is copied first.
  char command[CMD_MAX_LEN];
  if (length >= sizeof(command)) {
    _mqtt.publish(_topicResponses, "ERR: command too long");
    return;
  }
  memcpy(command, payload, length);
  command[length] = '\0';

  const String response = _commands.process(command);
  _mqtt.publish(_topicResponses, response.c_str());
}

void ConnectivityManager::publishTelemetry() {
  JsonDocument doc;
  _state.get().writeTelemetry(doc);
  char payload[MQTT_BUFFER_SIZE / 2];
  serializeJson(doc, payload, sizeof(payload));
  _mqtt.publish(_topicTelemetry, payload);
}

void ConnectivityManager::buildTopics() {
  snprintf(_topicTelemetry, sizeof(_topicTelemetry), "%s" MQTT_SUFFIX_TELEMETRY, _settings.mqttTopic);
  snprintf(_topicCommands, sizeof(_topicCommands), "%s" MQTT_SUFFIX_COMMANDS, _settings.mqttTopic);
  snprintf(_topicResponses, sizeof(_topicResponses), "%s" MQTT_SUFFIX_RESPONSES, _settings.mqttTopic);
}

void ConnectivityManager::publishConnectivity() {
  WifiStatus wifi = WifiStatus::Disconnected;
  if (_wifiState == WifiState::Connected) wifi = WifiStatus::Station;
  if (_wifiState == WifiState::AccessPoint) wifi = WifiStatus::AccessPoint;
  const bool mqtt = _mqtt.connected();
  if (wifi == _publishedWifi && mqtt == _publishedMqtt) return;

  _publishedWifi = wifi;
  _publishedMqtt = mqtt;
  IPAddress ip;
  if (wifi == WifiStatus::Station) ip = WiFi.localIP();
  if (wifi == WifiStatus::AccessPoint) ip = WiFi.softAPIP();
  _state.setConnectivity(wifi, mqtt, ip.toString().c_str());
}

void ConnectivityManager::setupWebRoutes() {
  _web.on("/", HTTP_GET, [this]() { handleRoot(); });
  _web.on("/config", HTTP_GET, [this]() { handleConfig(); });
  _web.on("/status", HTTP_GET, [this]() { handleStatus(); });
  _web.on("/save", HTTP_POST, [this]() { handleSave(); });
  _web.on("/cmd", HTTP_ANY, [this]() { handleCommand(); });
  _web.onNotFound([this]() { handleNotFound(); });
}

void ConnectivityManager::handleRoot() {
  _web.send(200, "text/html", kPage);
}

void ConnectivityManager::handleConfig() {
  // The WiFi password is never sent back to the browser.
  JsonDocument doc;
  doc["ssid"] = _settings.wifiSsid;
  doc["broker"] = _settings.mqttHost;
  doc["port"] = _settings.mqttPort;
  doc["topic"] = _settings.mqttTopic;
  doc["lat"] = _settings.latitude;
  doc["lon"] = _settings.longitude;
  doc["tel"] = _settings.telemetryIntervalMs;
  String json;
  serializeJson(doc, json);
  _web.send(200, "application/json", json);
}

void ConnectivityManager::handleStatus() {
  _web.send(200, "application/json", _commands.process("get status"));
}

void ConnectivityManager::handleSave() {
  String errors;
  const auto addError = [&errors](const char *field) {
    if (!errors.isEmpty()) errors += ", ";
    errors += field;
  };

  const String password = _web.arg("pass");
  const char *newPassword = password.isEmpty() ? _settings.wifiPass : password.c_str();
  if (!_storage.setWifi(_web.arg("ssid").c_str(), newPassword)) addError("WiFi");

  uint32_t port = 0;
  if (!parseUnsignedArg(_web.arg("port"), port) || port > UINT16_MAX ||
      !_storage.setMqtt(_web.arg("broker").c_str(), port)) {
    addError("broker MQTT");
  }
  if (!_storage.setMqttTopic(_web.arg("topic").c_str())) addError("prefijo de tópicos");

  float latitude = 0.0f;
  float longitude = 0.0f;
  if (!parseFloatArg(_web.arg("lat"), latitude) || !parseFloatArg(_web.arg("lon"), longitude) ||
      !_storage.setLocation(latitude, longitude)) {
    addError("ubicación");
  }

  uint32_t telemetry = 0;
  if (!parseUnsignedArg(_web.arg("tel"), telemetry) || !_storage.setTelemetryInterval(telemetry)) {
    addError("telemetría");
  }

  // The browser clock sets the time when there is no internet access for NTP.
  const unsigned long epoch = strtoul(_web.arg("ep").c_str(), nullptr, 10);
  const bool timeSynced = epoch >= MIN_VALID_EPOCH;
  if (timeSynced) {
    const struct timeval now = {static_cast<time_t>(epoch), 0};
    settimeofday(&now, nullptr);
  }

  String message;
  if (errors.isEmpty()) {
    message = "Configuración guardada.";
  } else {
    message = "Valores inválidos, no guardados: ";
    message += errors;
    message += '.';
  }
  message += timeSynced ? " Hora sincronizada." : " Hora no sincronizada.";
  _web.send(200, "text/plain; charset=utf-8", message);
}

void ConnectivityManager::handleCommand() {
  _web.send(200, "text/plain", _commands.process(_web.arg("c").c_str()));
}

void ConnectivityManager::handleNotFound() {
  if (_wifiState != WifiState::AccessPoint) {
    _web.send(404, "text/plain", "Not found");
    return;
  }
  // Any URL requested by a phone's connectivity check opens the captive portal.
  _web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
  _web.send(302, "text/plain", "");
}
