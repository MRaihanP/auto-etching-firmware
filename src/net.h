#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32MQTTClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"
#include "data.h"
#include "storage.h"
#include "api.h"
#include "net_ca.h"

// Kredensial lokal. secrets.h di-ignore Git; clone baru memakai placeholder
// dari secrets.example.h sampai file tersebut disalin.
#if __has_include("../secrets.h")
#include "../secrets.h"
#endif
#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID  "PCB Expres V3"
#endif
#ifndef SECRET_WIFI_PASS
#define SECRET_WIFI_PASS  "CHANGE_ME_WIFI_PASSWORD"
#endif
#ifndef SECRET_MQTT_HOST
#define SECRET_MQTT_HOST  "mqtt.loggerindonesia.com"
#endif
#ifndef SECRET_MQTT_PORT
#define SECRET_MQTT_PORT  8883
#endif
#ifndef SECRET_MQTT_USER
#define SECRET_MQTT_USER  "CHANGE_ME_MQTT_USER"
#endif
#ifndef SECRET_MQTT_PASS
#define SECRET_MQTT_PASS  "CHANGE_ME_MQTT_PASSWORD"
#endif
#ifndef SECRET_MQTT_BASE
#define SECRET_MQTT_BASE  "pelarut"
#endif
#ifndef SECRET_OTA_PASS
#define SECRET_OTA_PASS   "CHANGE_ME_OTA_PASSWORD"
#endif

// TLS 8883 memakai CA ISRG Root X2. Matikan lewat -DNET_ENABLE_TLS=0
// hanya bila ingin build tanpa TLS/port 1883.
#ifndef NET_ENABLE_TLS
#define NET_ENABLE_TLS 1
#endif

// ================= MQTT / NETWORK (Phase 2) =================
// - WiFi/MQTT berjalan di background task ESP32MQTTClient (tidak memblokir loop).
// - Callback MQTT hanya memasukkan command ke queue; aktuator tetap di loop.
// - Default WiFi/MQTT sudah fixed; command wifi/mqtt hanya untuk diagnostik.

#define NET_WIFI_SSID_DEFAULT SECRET_WIFI_SSID
#define NET_WIFI_PASS_DEFAULT SECRET_WIFI_PASS
#define NET_HOST_DEFAULT      SECRET_MQTT_HOST
#define NET_PORT_DEFAULT      SECRET_MQTT_PORT
#define NET_USER_DEFAULT      SECRET_MQTT_USER
#define NET_PASS_DEFAULT      SECRET_MQTT_PASS
#define NET_BASE_DEFAULT      SECRET_MQTT_BASE
#define NET_VERSION      2

struct NetConfig {
  uint32_t versi;
  uint8_t  enabled;
  char     ssid[33];
  char     wifiPass[65];
  char     host[64];
  uint16_t port;
  char     mqttUser[32];
  char     mqttPass[64];
  char     baseTopic[32];
  char     deviceId[24];
  uint32_t crc;
};

NetConfig netCfg;
bool netStarted = false;
bool netMqttStarted = false;
volatile bool netPerluStatus = false;
unsigned long netLastWifiTry = 0;
unsigned long netLastStatusMs = 0;
unsigned long netLastTelemetryMs = 0;
unsigned long netNtpLastTry = 0;
int netLastEventHead = 0;
volatile uint8_t netWifiReason = 0;

char topicStatus[80];
char topicTelemetry[80];
char topicEvent[80];
char topicAck[80];
char topicCmd[80];
char topicOnline[80];
char topicPresets[80];

struct MqttCommand {
  char id[24];
  char cmd[16];
  int  arg;
  long ml;
  char payload[512];   // untuk remote config
};

QueueHandle_t netCmdQueue = nullptr;
ESP32MQTTClient mqttClient;

void netSetStr(char* dst, size_t n, const char* src) {
  if (dst == nullptr || n == 0) return;
  if (src == nullptr) { dst[0] = '\0'; return; }
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

void netDefaultDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(netCfg.deviceId, sizeof(netCfg.deviceId),
           "esp32-%06lX", (unsigned long)(mac & 0xFFFFFFULL));
}

void netDefault() {
  memset(&netCfg, 0, sizeof(netCfg));
  netCfg.versi = NET_VERSION;
  netCfg.enabled = 1;
  netCfg.port = NET_PORT_DEFAULT;
  netSetStr(netCfg.ssid, sizeof(netCfg.ssid), NET_WIFI_SSID_DEFAULT);
  netSetStr(netCfg.wifiPass, sizeof(netCfg.wifiPass), NET_WIFI_PASS_DEFAULT);
  netSetStr(netCfg.host, sizeof(netCfg.host), NET_HOST_DEFAULT);
  netSetStr(netCfg.mqttUser, sizeof(netCfg.mqttUser), NET_USER_DEFAULT);
  netSetStr(netCfg.mqttPass, sizeof(netCfg.mqttPass), NET_PASS_DEFAULT);
  netSetStr(netCfg.baseTopic, sizeof(netCfg.baseTopic), NET_BASE_DEFAULT);
  netDefaultDeviceId();
}

bool netMuat() {
  Preferences p;
  NetConfig d;
  bool valid = false;
  if (p.begin(PREF_NS, true)) {
    if (p.getBytesLength("net") == sizeof(d)) {
      p.getBytes("net", &d, sizeof(d));
      valid = (d.versi == NET_VERSION) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
    }
    p.end();
  }
  if (valid) {
    memcpy(&netCfg, &d, sizeof(netCfg));
    netCfg.host[sizeof(netCfg.host) - 1] = '\0';
    netCfg.ssid[sizeof(netCfg.ssid) - 1] = '\0';
    netCfg.wifiPass[sizeof(netCfg.wifiPass) - 1] = '\0';
    netCfg.mqttUser[sizeof(netCfg.mqttUser) - 1] = '\0';
    netCfg.mqttPass[sizeof(netCfg.mqttPass) - 1] = '\0';
    netCfg.baseTopic[sizeof(netCfg.baseTopic) - 1] = '\0';
    netCfg.deviceId[sizeof(netCfg.deviceId) - 1] = '\0';
    if (netCfg.deviceId[0] == '\0') netDefaultDeviceId();
    if (netCfg.baseTopic[0] == '\0') netSetStr(netCfg.baseTopic, sizeof(netCfg.baseTopic), NET_BASE_DEFAULT);
    if (netCfg.host[0] == '\0') netSetStr(netCfg.host, sizeof(netCfg.host), NET_HOST_DEFAULT);
    if (netCfg.port == 0) netCfg.port = NET_PORT_DEFAULT;
    // Config lama tanpa WiFi: isi default agar alat langsung bisa connect.
    if (netCfg.ssid[0] == '\0') {
      netSetStr(netCfg.ssid, sizeof(netCfg.ssid), NET_WIFI_SSID_DEFAULT);
      netSetStr(netCfg.wifiPass, sizeof(netCfg.wifiPass), NET_WIFI_PASS_DEFAULT);
      netCfg.enabled = 1;
    }
  } else {
    netDefault();
    dbg.println(F("[NET] Config belum ada -> default"));
  }
  return valid;
}

bool simpanNet() {
  netCfg.versi = NET_VERSION;
  netCfg.crc = hitungCrc((const uint8_t*)&netCfg, sizeof(netCfg) - sizeof(netCfg.crc));
  Preferences p;
  bool ok = false;
  if (p.begin(PREF_NS, false)) {
    ok = (p.putBytes("net", &netCfg, sizeof(netCfg)) == sizeof(netCfg));
    p.end();
  }
  return ok;
}

void netBangunTopik() {
  char base[32];
  netSetStr(base, sizeof(base), netCfg.baseTopic[0] ? netCfg.baseTopic : NET_BASE_DEFAULT);
  int blen = strlen(base);
  while (blen > 1 && base[blen - 1] == '/') base[--blen] = '\0';

  snprintf(topicStatus,    sizeof(topicStatus),    "%s/%s/status",    base, netCfg.deviceId);
  snprintf(topicTelemetry, sizeof(topicTelemetry), "%s/%s/telemetry", base, netCfg.deviceId);
  snprintf(topicEvent,     sizeof(topicEvent),     "%s/%s/event",     base, netCfg.deviceId);
  snprintf(topicAck,       sizeof(topicAck),       "%s/%s/ack",       base, netCfg.deviceId);
  snprintf(topicCmd,       sizeof(topicCmd),       "%s/%s/cmd",       base, netCfg.deviceId);
  snprintf(topicOnline,    sizeof(topicOnline),    "%s/%s/online",    base, netCfg.deviceId);
  snprintf(topicPresets,   sizeof(topicPresets),   "%s/%s/presets",   base, netCfg.deviceId);
}

// ---------- command queue ----------
void netCallbackPayload(const char* payload, size_t len) {
  if (netCmdQueue == nullptr) return;

  MqttCommand c;
  memset(&c, 0, sizeof(c));
  c.arg = -1;
  c.ml = -1;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) {
    netSetStr(c.id, sizeof(c.id), "-");
    netSetStr(c.cmd, sizeof(c.cmd), "invalid");
  } else {
    netSetStr(c.id, sizeof(c.id), doc["id"] | "-");
    netSetStr(c.cmd, sizeof(c.cmd), doc["cmd"] | "");
    if (doc["preset"].is<int>()) c.arg = doc["preset"];
    else if (doc["idx"].is<int>()) c.arg = doc["idx"];
    c.ml = doc["ml"] | -1L;
    if (strcmp(c.cmd, "config") == 0 && doc["payload"].is<JsonObject>()) {
      serializeJson(doc["payload"], c.payload, sizeof(c.payload));
    } else {
      netSetStr(c.payload, sizeof(c.payload), payload);
    }
  }
  xQueueSend(netCmdQueue, &c, 0);
}

void netPublishPresets();

void onMqttConnect(esp_mqtt_client_handle_t client) {
  if (!mqttClient.isMyTurn(client)) return;
  mqttClient.subscribe(topicCmd, [](const std::string& payload) {
    netCallbackPayload(payload.c_str(), payload.size());
  }, 1);
  mqttClient.publish(topicOnline, std::string("1"), 1, true);
  netPublishPresets();
  netPerluStatus = true;
  // Kirim ulang maksimal 5 event terakhir setelah reconnect.
  int nEvent = eventCount < 5 ? eventCount : 5;
  netLastEventHead = (eventHead - nEvent + EVENT_MAX) % EVENT_MAX;
  netLastStatusMs = 0;
  netLastTelemetryMs = 0;
  dbg.println(F("[NET] MQTT terhubung, subscribe cmd"));
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  mqttClient.onEventCallback(event);
  return ESP_OK;
}
#else
void handleMQTT(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
  auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);
  mqttClient.onEventCallback(event);
}
#endif

// ---------- publish ----------
void netPublishStatus() {
  if (!mqttClient.isConnected()) return;
  static char buf[4096];
  apiBuatStatusJson(buf, sizeof(buf));
  if (strlen(buf) < 16) return;
  mqttClient.publish(topicStatus, std::string(buf), 0, true);
}

void netPublishTelemetry() {
  if (!mqttClient.isConnected()) return;
  static char buf[4096];
  apiBuatStatusJson(buf, sizeof(buf));
  if (strlen(buf) < 16) return;
  mqttClient.publish(topicTelemetry, std::string(buf), 0, false);
}

void netPublishPresets() {
  if (!mqttClient.isConnected()) return;
  apiBuatPresetsJson(apiPresetJsonBuf, sizeof(apiPresetJsonBuf));
  if (strlen(apiPresetJsonBuf) < 16) return;
  mqttClient.publish(topicPresets, std::string(apiPresetJsonBuf), 0, true);
}

void netPublishEventAt(int idx) {
  if (!mqttClient.isConnected() || idx < 0 || idx >= EVENT_MAX) return;
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"t\":%lu,\"epoch\":%lu,\"kode\":%u,\"detail\":\"%s\"}",
           (unsigned long)eventLog[idx].ms,
           (unsigned long)eventLog[idx].epoch,
           (unsigned)eventLog[idx].kode,
           eventLog[idx].detail);
  mqttClient.publish(topicEvent, std::string(buf), 0, false);
}

void netPublishAck(const MqttCommand& c, bool ok, const char* msg) {
  if (!mqttClient.isConnected()) return;
  JsonDocument doc;
  doc["id"] = c.id[0] ? c.id : "-";
  doc["ok"] = ok;
  doc["cmd"] = c.cmd;
  doc["msg"] = msg ? msg : "";
  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  mqttClient.publish(topicAck, std::string(buf), 1, false);
}

void netPublishStatusJson() {
  netPublishStatus();
}

// ---------- queue processing (loop task) ----------
void netProsesQueue() {
  if (netCmdQueue == nullptr) return;
  static unsigned long lastCmdMs = 0;
  MqttCommand c;
  while (xQueueReceive(netCmdQueue, &c, 0) == pdTRUE) {
    unsigned long now = millis();
    if (now - lastCmdMs < 200UL) {
      eventCatat(121, "RATE LIMIT");
      netPublishAck(c, false, "rate limit");
      continue;
    }
    lastCmdMs = now;
    eventCatat(120, c.cmd);

    if (strcmp(c.cmd, "clear") == 0) {
      // Clear E-stop dari dashboard hanya boleh setelah tombol fisik lepas
      // dan evakuasi cairan selesai. Error lain tetap harus lokal.
      if (!safetyClearRemoteBoleh()) {
        netPublishAck(c, false, "lepas E-stop fisik dulu");
        continue;
      }
      char msg[40];
      bool ok = perintahProses(CMD_CLEAR, -1, msg, sizeof(msg));
      netPublishAck(c, ok, msg);
      continue;
    }
    if (strcmp(c.cmd, "config") == 0) {
      char msg[48];
      bool ok = apiKonfigurasiJson(c.payload, msg, sizeof(msg));
      if (ok) netPublishPresets();
      netPublishAck(c, ok, msg);
      continue;
    }
    if (strcmp(c.cmd, "presets") == 0) {
      netPublishPresets();
      netPublishAck(c, true, "presets");
      continue;
    }
    if (strcmp(c.cmd, "tank") == 0) {
      char msg[40];
      bool ok = apiSetTank(c.arg, c.ml, msg, sizeof(msg));
      netPublishAck(c, ok, msg);
      continue;
    }

    PerintahProses p;
    bool dikenal = true;
    if      (strcmp(c.cmd, "start") == 0)  p = CMD_START;
    else if (strcmp(c.cmd, "pause") == 0)  p = CMD_PAUSE;
    else if (strcmp(c.cmd, "resume") == 0) p = CMD_RESUME;
    else if (strcmp(c.cmd, "cancel") == 0) p = CMD_CANCEL;
    else if (strcmp(c.cmd, "drain") == 0)  p = CMD_DRAIN;
    else if (strcmp(c.cmd, "clear") == 0)  p = CMD_CLEAR;
    else if (strcmp(c.cmd, "preset") == 0) p = CMD_PRESET;
    else if (strcmp(c.cmd, "pcb") == 0)    p = CMD_PCB;
    else if (strcmp(c.cmd, "status") == 0) {
      netPublishStatus();
      netPublishAck(c, true, "status");
      continue;
    } else {
      dikenal = false;
    }

    if (!dikenal) {
      netPublishAck(c, false, "perintah tidak dikenal");
      continue;
    }

    char msg[64];
    bool ok = perintahProses(p, c.arg, msg, sizeof(msg));
    netPublishAck(c, ok, msg);
  }
}

// ---------- init/update ----------
void netWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    netWifiReason = info.wifi_sta_disconnected.reason;
    dbg.print(F("[NET] WiFi terputus, reason="));
    dbg.println((int)netWifiReason);
  } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    netWifiReason = 0;
    dbg.println(F("[NET] WiFi terhubung ke AP, menunggu IP..."));
  }
}

void netMulai() {
  if (netStarted || !netCfg.enabled || netCfg.ssid[0] == '\0') return;

  netBangunTopik();
  if (netCmdQueue == nullptr) {
    netCmdQueue = xQueueCreate(8, sizeof(MqttCommand));
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.onEvent(netWifiEvent);
  WiFi.begin(netCfg.ssid, netCfg.wifiPass);

  // TLS membutuhkan waktu sistem yang benar. Mulai SNTP sebelum MQTT
  // mencoba handshake; ESP-MQTT akan reconnect setelah waktu tersinkron.
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  mqttClient.setURL(netCfg.host, netCfg.port, netCfg.mqttUser, netCfg.mqttPass);
#if NET_ENABLE_TLS
  if (netCfg.port == 8883) mqttClient.setCaCert(NET_CA_CERT);
#endif
  mqttClient.setMqttClientName(netCfg.deviceId);
  mqttClient.setKeepAlive(30);
  mqttClient.setMaxPacketSize(8192);
  mqttClient.setMaxOutPacketSize(8192);
  mqttClient.setTaskPrio(1);
  mqttClient.enableLastWillMessage(topicOnline, "0", true, 1);
  netStarted = true;
  netMqttStarted = false;   // dimulai setelah WiFi+NTP siap

  dbg.print(F("[NET] Konfigurasi MQTT ke "));
  dbg.print(netCfg.host);
  dbg.print(':');
  dbg.println(netCfg.port);
}

void netStatusJson() {
  Serial.print(F("{\"ok\":true,\"cmd\":\"net\",\"enabled\":"));
  Serial.print(netCfg.enabled ? F("true") : F("false"));
  Serial.print(F(",\"started\":"));
  Serial.print(netStarted ? F("true") : F("false"));
  Serial.print(F(",\"mqtt_started\":"));
  Serial.print(netMqttStarted ? F("true") : F("false"));
  Serial.print(F(",\"wifi\":"));
  Serial.print(WiFi.status() == WL_CONNECTED ? F("true") : F("false"));
  Serial.print(F(",\"wifi_status\":"));
  Serial.print((int)WiFi.status());
  Serial.print(F(",\"wifi_reason\":"));
  Serial.print((int)netWifiReason);
  Serial.print(F(",\"ssid\":\""));
  Serial.print(netCfg.ssid);
  Serial.print(F("\""));
  Serial.print(F(",\"mqtt\":"));
  Serial.print(mqttClient.isConnected() ? F("true") : F("false"));
  Serial.print(F(",\"ip\":\""));
  Serial.print(WiFi.localIP().toString());
  Serial.print(F("\",\"rssi\":"));
  Serial.print(WiFi.RSSI());
  Serial.print(F(",\"host\":\""));
  Serial.print(netCfg.host);
  Serial.print(F("\",\"port\":"));
  Serial.print(netCfg.port);
  Serial.print(F(",\"tls\":"));
  Serial.print((NET_ENABLE_TLS && netCfg.port == 8883) ? F("true") : F("false"));
  Serial.print(F(",\"device\":\""));
  Serial.print(netCfg.deviceId);
  Serial.print(F("\",\"base\":\""));
  Serial.print(netCfg.baseTopic);
  Serial.println(F("\"}"));
}

void netInit() {
  netMuat();
  // Migrasi SSID lama (dua "s" atau terpotong "PCB") ke nama AP yang benar.
  if (strcmp(netCfg.ssid, "PCB Express V3") == 0 ||
      strcmp(netCfg.ssid, "PCB") == 0) {
    netSetStr(netCfg.ssid, sizeof(netCfg.ssid), NET_WIFI_SSID_DEFAULT);
    netSetStr(netCfg.wifiPass, sizeof(netCfg.wifiPass), NET_WIFI_PASS_DEFAULT);
    simpanNet();
    dbg.println(F("[NET] Migrasi SSID ke PCB Expres V3"));
  }
  netBangunTopik();
  if (netCfg.enabled && netCfg.ssid[0] != '\0') {
    netMulai();
  } else {
    dbg.println(F("[NET] Nonaktif. Provisioning: wifi <ssid> <pass> lalu net on"));
  }
}

void netUpdate() {
  if (!netCfg.enabled) return;

  unsigned long now = millis();
  static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
  static uint8_t lastWifiReason = 0;
  wl_status_t wifiStatus = WiFi.status();
  uint8_t wifiReason = netWifiReason;
  if (wifiStatus != lastWifiStatus || wifiReason != lastWifiReason) {
    lastWifiStatus = wifiStatus;
    lastWifiReason = wifiReason;
    dbg.print(F("[NET] WiFi status="));
    dbg.print((int)wifiStatus);
    dbg.print(F(" reason="));
    dbg.println((int)wifiReason);
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (netCfg.ssid[0] != '\0' && now - netLastWifiTry >= 10000UL) {
      netLastWifiTry = now;
      WiFi.reconnect();
      dbg.print(F("[NET] Reconnect WiFi SSID=\""));
      dbg.print(netCfg.ssid);
      dbg.println(F("\"..."));
    }
    return;
  }

  // Sinkron waktu untuk timestamp event/telemetry dan validasi TLS.
  if (time(nullptr) < 1767225600L && now - netNtpLastTry >= 60000UL) {
    netNtpLastTry = now;
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    dbg.println(F("[NET] Sinkron NTP..."));
  }

  if (!netMqttStarted) {
    if (NET_ENABLE_TLS && netCfg.port == 8883 && time(nullptr) < 1767225600L) {
      return;   // TLS butuh waktu valid
    }
    mqttClient.loopStart();
    netMqttStarted = true;
    dbg.println(F("[NET] MQTT task dimulai"));
  }

  netProsesQueue();

  if (!mqttClient.isConnected()) return;

  if (netPerluStatus || netLastStatusMs == 0 || now - netLastStatusMs >= 10000UL) {
    netLastStatusMs = now;
    netPerluStatus = false;
    netPublishStatus();
  }

  if (netLastTelemetryMs == 0 || now - netLastTelemetryMs >= 5000UL) {
    netLastTelemetryMs = now;
    netPublishTelemetry();
  }

  int guard = 0;
  while (netLastEventHead != eventHead && guard++ < 4) {
    netPublishEventAt(netLastEventHead);
    netLastEventHead = (netLastEventHead + 1) % EVENT_MAX;
  }
}

// ---------- Serial provisioning ----------
bool netTanganiBaris(char* baris) {
  if (baris == nullptr) return false;

  if (strcmp(baris, "wifiscan") == 0) {
    if (prosesJalan) {
      apiBalas(false, "wifiscan", "proses berjalan");
      return true;
    }
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      Serial.print(F("[WIFI] "));
      Serial.print(i);
      Serial.print(F(": \""));
      Serial.print(WiFi.SSID(i));
      Serial.print(F("\" "));
      Serial.print(WiFi.RSSI(i));
      Serial.print(F("dBm ch"));
      Serial.println(WiFi.channel(i));
    }
    WiFi.scanDelete();
    Serial.print(F("{\"ok\":true,\"cmd\":\"wifiscan\",\"count\":"));
    Serial.print(n);
    Serial.println(F("}"));
    return true;
  }

  if (strncmp(baris, "wifi ", 5) == 0) {
    char* p = baris + 5;

    if (strcmp(p, "default") == 0) {
      netSetStr(netCfg.ssid, sizeof(netCfg.ssid), NET_WIFI_SSID_DEFAULT);
      netSetStr(netCfg.wifiPass, sizeof(netCfg.wifiPass), NET_WIFI_PASS_DEFAULT);
      netCfg.enabled = 1;
      simpanNet();
      netBangunTopik();
      if (!netStarted) netMulai();
      else {
        WiFi.disconnect();
        WiFi.begin(netCfg.ssid, netCfg.wifiPass);
      }
      apiBalas(true, "wifi", "default dipasang, mencoba connect");
      return true;
    }

    // SSID boleh mengandung spasi; password adalah token terakhir.
    char* sp = strrchr(p, ' ');
    if (sp == nullptr || sp == p) {
      apiBalas(false, "wifi", "format: wifi <ssid> <pass>");
      return true;
    }
    *sp = '\0';
    netSetStr(netCfg.ssid, sizeof(netCfg.ssid), p);
    netSetStr(netCfg.wifiPass, sizeof(netCfg.wifiPass), sp + 1);
    netCfg.enabled = 1;
    simpanNet();
    netBangunTopik();
    if (!netStarted) {
      netMulai();
    } else {
      WiFi.disconnect();
      WiFi.begin(netCfg.ssid, netCfg.wifiPass);
    }
    apiBalas(true, "wifi", "disimpan, mencoba connect");
    return true;
  }

  if (strncmp(baris, "mqtt", 4) == 0 &&
      (baris[4] == '\0' || baris[4] == ' ')) {
    char* host = strtok(baris + 4, " ");
    if (host == nullptr) {
      Serial.print(F("{\"ok\":true,\"cmd\":\"mqtt\",\"host\":\""));
      Serial.print(netCfg.host);
      Serial.print(F("\",\"port\":"));
      Serial.print(netCfg.port);
      Serial.print(F(",\"user\":\""));
      Serial.print(netCfg.mqttUser);
      Serial.println(F("\"}"));
      return true;
    }
    char* port = strtok(nullptr, " ");
    char* user = strtok(nullptr, " ");
    char* pass = strtok(nullptr, " ");
    if (!port || !user || !pass) {
      apiBalas(false, "mqtt", "format: mqtt <host> <port> <user> <pass>");
      return true;
    }
    int portNum = atoi(port);
    if (portNum < 1 || portNum > 65535) {
      apiBalas(false, "mqtt", "port 1..65535");
      return true;
    }
    netSetStr(netCfg.host, sizeof(netCfg.host), host);
    netCfg.port = (uint16_t)portNum;
    netSetStr(netCfg.mqttUser, sizeof(netCfg.mqttUser), user);
    netSetStr(netCfg.mqttPass, sizeof(netCfg.mqttPass), pass);
    simpanNet();
    apiBalas(true, "mqtt", netStarted ? "disimpan, reboot untuk apply" : "disimpan");
    return true;
  }

  if (strncmp(baris, "device ", 7) == 0) {
    if (baris[7] == '\0') {
      apiBalas(false, "device", "device id kosong");
      return true;
    }
    netSetStr(netCfg.deviceId, sizeof(netCfg.deviceId), baris + 7);
    netBangunTopik();
    simpanNet();
    apiBalas(true, "device", netStarted ? "disimpan, reboot untuk apply" : "disimpan");
    return true;
  }

  if (strncmp(baris, "topic ", 6) == 0) {
    if (baris[6] == '\0') {
      apiBalas(false, "topic", "base topic kosong");
      return true;
    }
    netSetStr(netCfg.baseTopic, sizeof(netCfg.baseTopic), baris + 6);
    netBangunTopik();
    simpanNet();
    apiBalas(true, "topic", netStarted ? "disimpan, reboot untuk apply" : "disimpan");
    return true;
  }

  if (strcmp(baris, "net") == 0) {
    netStatusJson();
    return true;
  }
  if (strcmp(baris, "net on") == 0) {
    if (netCfg.ssid[0] == '\0') {
      apiBalas(false, "net", "set wifi dulu");
      return true;
    }
    netCfg.enabled = 1;
    simpanNet();
    if (!netStarted) netMulai();
    else if (WiFi.status() != WL_CONNECTED) WiFi.begin(netCfg.ssid, netCfg.wifiPass);
    apiBalas(true, "net", "on");
    return true;
  }
  if (strcmp(baris, "net off") == 0) {
    netCfg.enabled = 0;
    simpanNet();
    WiFi.disconnect(true);
    apiBalas(true, "net", "off");
    return true;
  }
  if (strcmp(baris, "net save") == 0) {
    bool ok = simpanNet();
    apiBalas(ok, "net", ok ? "saved" : "gagal simpan");
    return true;
  }

  return false;
}
