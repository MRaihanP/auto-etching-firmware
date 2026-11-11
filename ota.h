#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "data.h"
#include "net.h"
#include "process.h"
#include "safety.h"

// ================= OTA HTTP RINGAN =================
// POST /update dengan body binary firmware dan header:
//   X-OTA-Password: <OTA_PASSWORD>
// Contoh (satu baris):
//   curl -X POST --data-binary @firmware.bin -H "X-OTA-Password: <OTA_PASSWORD>" http://<IP>:3232/update
// Hanya dilayani saat alat IDLE dan safety AMAN. Menggantikan ArduinoOTA
// untuk menghemat flash (~46 KB).

#define OTA_HTTP_PORT 3232
#define OTA_PASS_DEFAULT SECRET_OTA_PASS

WiFiServer otaServer(OTA_HTTP_PORT);
bool otaStarted = false;

void otaStatusMuat() {
  Preferences p;
  if (p.begin(PREF_NS, true)) {
    otaHasilTerakhir = p.getInt("otastat", 0);
    p.end();
  }
}

void otaStatusSimpan(int hasil) {
  otaHasilTerakhir = hasil;
  Preferences p;
  if (p.begin(PREF_NS, false)) {
    p.putInt("otastat", hasil);
    p.end();
  }
}

void otaKirim(WiFiClient& c, int code, const char* msg) {
  const char* status = (code == 200) ? "OK" :
                       (code == 400) ? "Bad Request" :
                       (code == 401) ? "Unauthorized" :
                       (code == 404) ? "Not Found" :
                       (code == 409) ? "Conflict" : "Error";
  char resp[192];
  snprintf(resp, sizeof(resp),
           "HTTP/1.1 %d %s\r\nContent-Type: text/plain\r\n"
           "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
           code, status, (int)strlen(msg), msg);
  c.print(resp);
}

bool otaBacaBaris(WiFiClient& c, char* buf, size_t n, unsigned long timeoutMs) {
  size_t i = 0;
  unsigned long mulai = millis();
  while (millis() - mulai < timeoutMs) {
    if (!c.available()) continue;
    char ch = (char)c.read();
    if (ch == '\n') {
      if (i > 0 && buf[i - 1] == '\r') i--;
      buf[i] = '\0';
      return true;
    }
    if (i < n - 1) buf[i++] = ch;
  }
  buf[0] = '\0';
  return false;
}

void otaUpdate() {
  static bool statusDimuat = false;
  if (!statusDimuat) {
    otaStatusMuat();
    statusDimuat = true;
  }
  if (WiFi.status() != WL_CONNECTED) return;
  if (!otaStarted) {
    otaServer.begin();
    otaStarted = true;
    dbg.print(F("[OTA] HTTP siap di port "));
    dbg.println(OTA_HTTP_PORT);
  }

  WiFiClient client = otaServer.accept();
  if (!client) return;
  client.setTimeout(5000);

  char line[160];
  if (!otaBacaBaris(client, line, sizeof(line), 5000)) { client.stop(); return; }
  if (strncmp(line, "POST /update", 12) != 0) {
    otaKirim(client, 404, "Gunakan POST /update");
    client.stop();
    return;
  }

  long contentLen = -1;
  bool auth = false;
  while (otaBacaBaris(client, line, sizeof(line), 5000)) {
    if (line[0] == '\0') break;
    if (strncasecmp(line, "Content-Length:", 15) == 0) {
      contentLen = atol(line + 15);
    } else if (strncasecmp(line, "X-OTA-Password:", 15) == 0) {
      char* p = line + 15;
      while (*p == ' ') p++;
      if (strcmp(p, OTA_PASS_DEFAULT) == 0) auth = true;
    }
  }

  if (!auth) {
    otaKirim(client, 401, "Password OTA salah");
    client.stop();
    return;
  }
  if (prosesJalan || evakuasiAktif || safetyState != SAF_AMAN) {
    otaKirim(client, 409, "Alat tidak IDLE/AMAN");
    client.stop();
    return;
  }
  if (contentLen <= 0) {
    otaKirim(client, 400, "Content-Length tidak valid");
    client.stop();
    return;
  }
  if (!Update.begin((size_t)contentLen)) {
    otaKirim(client, 500, "Update.begin gagal");
    client.stop();
    return;
  }

  otaAktif = true;
  eventCatat(130, "OTA START");
  dbg.print(F("[OTA] Mulai, "));
  dbg.print(contentLen);
  dbg.println(F(" byte"));

  size_t ditulis = 0;
  unsigned long terakhir = millis();
  while (ditulis < (size_t)contentLen) {
    if (client.available()) {
      uint8_t buf[1024];
      size_t sisa = (size_t)contentLen - ditulis;
      size_t n = client.readBytes(buf, sisa < sizeof(buf) ? sisa : sizeof(buf));
      if (n == 0) continue;
      if (Update.write(buf, n) != n) break;
      ditulis += n;
      terakhir = millis();
      esp_task_wdt_reset();
    } else if (millis() - terakhir > 10000UL) {
      break;
    }
  }

  bool ok = (ditulis == (size_t)contentLen) && !Update.hasError();
  if (ok) ok = Update.end(true);
  else Update.abort();

  otaAktif = false;
  if (ok) {
    eventCatat(131, "OTA END");
    otaStatusSimpan(1);
    otaKirim(client, 200, "OK, restart");
    client.stop();
    delay(500);
    ESP.restart();
  } else {
    eventCatat(132, "OTA ERROR");
    otaStatusSimpan(2);
    otaKirim(client, 500, "Update gagal");
    client.stop();
    dbg.print(F("[OTA] Gagal, ditulis "));
    dbg.println(ditulis);
  }
}
