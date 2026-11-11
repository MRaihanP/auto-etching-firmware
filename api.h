#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "motor.h"
#include "dcmotor.h"
#include "storage.h"
#include "safety.h"
#include "process.h"

// ================= FONDASI IoT: COMMAND API + SNAPSHOT + EVENT LOG =================
// Transport saat ini: Serial non-blocking dengan perintah baris.
// Lapisan WiFi/MQTT/HTTPS nanti cukup memanggil perintahProses() dan
// mengirim isi StatusSnapshot; tidak perlu menyentuh aktuator langsung.
// Prinsip: LOCAL-FIRST. Jaringan tidak boleh memblokir loop atau menghentikan
// proses. Remote start tetap berhenti di WAIT_PCB sampai ENTER lokal.

enum PerintahProses {
  CMD_STATUS = 0,
  CMD_EVENTS,
  CMD_START,
  CMD_PAUSE,
  CMD_RESUME,
  CMD_CANCEL,
  CMD_DRAIN,
  CMD_CLEAR,
  CMD_PRESET,
  CMD_PCB,
  CMD_HELP
};

struct StatusSnapshot {
  uint32_t uptimeMs;
  const char* versi;
  const char* resetReason;

  int  fase;
  int  langkah;
  int  siklus;
  int  siklusTotal;
  int  presetIdx;
  const char* presetNama;
  int  volumeWadah;
  bool jalan;
  bool jeda;
  bool tungguPcb;
  bool konfirmasi;
  bool pemulihanTersedia;

  int safetyState;
  int safetyKode;

  const char* dosingNama;
  int dosingTarget;
  int dosingSudah;

  float suhu[JUMLAH_SUHU];
  bool  suhuValid[JUMLAH_SUHU];

  int  levelPersen[JUMLAH_SENSOR];
  bool levelBasah[JUMLAH_SENSOR];
  bool levelFault[JUMLAH_SENSOR];

  long tankMl[5];
  int  tankKap[5];

  bool  pumpKalib[3];
  float dcRate[2];
  float stepMlPerRev[3];
  int   stepRpm[3];
  float dcC[2];
  int   dcDuty[2];
  int   levelKalibKosong[JUMLAH_SENSOR];
  int   levelKalibPenuh[JUMLAH_SENSOR];
  bool  levelKalibOk[JUMLAH_SENSOR];
  int   suhuAddr[JUMLAH_SUHU];
  float suhuOffsetX10[JUMLAH_SUHU];
  int   mqNormal;
  int   mqKotor;
  int   mqAktif;
  int   agitasiDuty[3];
  uint32_t kalibEpoch[9];

  uint32_t heapFree;
  uint32_t heapMin;
};

void isiSnapshot(StatusSnapshot& s) {
  s.uptimeMs = millis();
  s.versi = FW_VERSI;
  s.resetReason = infoAlasanReset;

  s.fase = (int)prosesFase;
  s.langkah = (int)prosesLangkah;
  s.siklus = prosesSiklus;
  s.siklusTotal = daftarPreset[presetTerpilih].siklus;
  s.presetIdx = presetTerpilih;
  s.presetNama = daftarPreset[presetTerpilih].nama;
  s.volumeWadah = volumeWadah;
  s.jalan = prosesJalan;
  s.jeda = prosesJeda;
  s.tungguPcb = prosesTungguPcb;
  s.konfirmasi = prosesKonfirmasi;
  s.pemulihanTersedia = pemulihanTersedia;

  s.safetyState = (int)safetyState;
  s.safetyKode = safetyKode;

  const char* dn = nullptr;
  int dt = 0, ds = 0;
  if (prosesInfoDosing(&dn, &dt, &ds)) {
    s.dosingNama = dn;
    s.dosingTarget = dt;
    s.dosingSudah = ds;
  } else {
    s.dosingNama = "";
    s.dosingTarget = 0;
    s.dosingSudah = 0;
  }

  for (int i = 0; i < JUMLAH_SUHU; i++) {
    s.suhu[i] = sensorSuhu(i);
    s.suhuValid[i] = sensorSuhuValid(i);
  }
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    s.levelPersen[i] = sensorPersen[i];
    s.levelBasah[i] = levelBasah(i);
    s.levelFault[i] = levelFault(i);
  }

  long tankTmp[5] = { tankHclMl, tankH2o2Ml, tankH2oMl,
                      tankSabunMl, tankLimbahMl };
  for (int i = 0; i < 5; i++) {
    s.tankMl[i] = tankTmp[i];
    s.tankKap[i] = kapasitasTank(i);
  }
  for (int i = 0; i < 3; i++) {
    s.pumpKalib[i] = pompaSiap(i);
    s.stepMlPerRev[i] = kalibStepperMl[i];
    s.stepRpm[i] = kalibStepperRpm[i];
  }
  for (int i = 0; i < 2; i++) {
    s.dcRate[i] = kalibDcMlPerSec[i];
    s.dcC[i] = kalibDcC[i];
    s.dcDuty[i] = kalibDutyDC[i];
  }
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    s.levelKalibKosong[i] = sensorKalibKosong[i];
    s.levelKalibPenuh[i] = sensorKalibPenuh[i];
    s.levelKalibOk[i] = sensorKalibSimpan[i];
  }
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    s.suhuAddr[i] = kalibSuhuAddr[i];
    s.suhuOffsetX10[i] = suhuOffsetX10[i];
  }
  s.mqNormal = mq135Normal;
  s.mqKotor = mq135Kotor;
  s.mqAktif = pengaturan.mqAktif;
  for (int i = 0; i < 3; i++) s.agitasiDuty[i] = agitasiSpdDuty[i];
  for (int i = 0; i < 9; i++) s.kalibEpoch[i] = kalibEpoch[i];

  s.heapFree = ESP.getFreeHeap();
  s.heapMin = ESP.getMinFreeHeap();
}

// Bangun status JSON ke buffer. Dipakai Serial dan MQTT.
void apiBuatStatusJson(char* out, size_t n) {
  if (out == nullptr || n == 0) return;
  StatusSnapshot s;
  isiSnapshot(s);

  JsonDocument doc;
  doc["ok"] = true;
  doc["cmd"] = "status";
  doc["ver"] = s.versi;
  doc["uptime"] = s.uptimeMs;
  doc["epoch"] = (uint32_t)time(nullptr);
  doc["reset"] = s.resetReason;
  doc["fase"] = s.fase;
  doc["fase_nama"] = namaFase[s.fase];
  doc["langkah"] = s.langkah;
  doc["langkah_nama"] = namaLangkah[s.langkah];
  doc["siklus"] = s.siklus;
  doc["siklus_total"] = s.siklusTotal;
  doc["preset"] = s.presetIdx;
  doc["preset_nama"] = s.presetNama;
  doc["volume_ml"] = s.volumeWadah;
  doc["jalan"] = s.jalan;
  doc["jeda"] = s.jeda;
  doc["tunggu_pcb"] = s.tungguPcb;
  doc["konfirmasi"] = s.konfirmasi;
  doc["pemulihan"] = s.pemulihanTersedia;
  doc["ota_aktif"] = otaAktif;
  doc["ota_hasil"] = otaHasilTerakhir;

  JsonObject safety = doc["safety"].to<JsonObject>();
  safety["state"] = s.safetyState;
  safety["kode"] = s.safetyKode;
  safety["nama"] = safetyInfo(s.safetyKode).nama;
  safety["detail"] = safetyDetail[0] ? safetyDetail : "";
  safety["aksi"] = safetyAksi[0] ? safetyAksi : "";
  safety["pin_high"] = (digitalRead(PIN_ESTOP) == HIGH);
  safety["clear_remote"] = safetyClearRemoteBoleh();

  JsonObject dosis = doc["dosis"].to<JsonObject>();
  dosis["nama"] = s.dosingNama;
  dosis["target"] = s.dosingTarget;
  dosis["sudah"] = s.dosingSudah;

  JsonArray suhu = doc["suhu"].to<JsonArray>();
  for (int i = 0; i < JUMLAH_SUHU; i++) suhu.add(s.suhu[i]);
  JsonArray suhuValid = doc["suhu_valid"].to<JsonArray>();
  for (int i = 0; i < JUMLAH_SUHU; i++) suhuValid.add(s.suhuValid[i]);

  JsonArray level = doc["level"].to<JsonArray>();
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    JsonObject o = level.add<JsonObject>();
    o["p"] = s.levelPersen[i];
    o["basah"] = s.levelBasah[i];
    o["fault"] = s.levelFault[i];
  }

  JsonArray tank = doc["tank"].to<JsonArray>();
  for (int i = 0; i < 5; i++) {
    JsonObject o = tank.add<JsonObject>();
    o["ml"] = s.tankMl[i];
    o["kap"] = s.tankKap[i];
  }

  JsonObject kalib = doc["kalib"].to<JsonObject>();
  JsonArray stepper = kalib["stepper"].to<JsonArray>();
  for (int i = 0; i < 3; i++) stepper.add(s.pumpKalib[i]);
  JsonArray dc = kalib["dc"].to<JsonArray>();
  dc.add(s.dcRate[0]);
  dc.add(s.dcRate[1]);
  JsonArray mlPerRev = kalib["ml_per_rev"].to<JsonArray>();
  JsonArray stepRpm = kalib["rpm"].to<JsonArray>();
  JsonArray stepEpoch = kalib["stepper_epoch"].to<JsonArray>();
  for (int i = 0; i < 3; i++) {
    mlPerRev.add(s.stepMlPerRev[i]);
    stepRpm.add(s.stepRpm[i]);
    stepEpoch.add(s.kalibEpoch[i]);
  }
  JsonArray dcDuty = kalib["dc_duty"].to<JsonArray>();
  JsonArray dcEpoch = kalib["dc_epoch"].to<JsonArray>();
  for (int i = 0; i < 2; i++) {
    dcDuty.add(s.dcDuty[i]);
    dcEpoch.add(s.kalibEpoch[3 + i]);
  }
  kalib["agitasi_epoch"] = s.kalibEpoch[5];
  JsonArray agitasiDuty = kalib["agitasi_duty"].to<JsonArray>();
  for (int i = 0; i < 3; i++) agitasiDuty.add(s.agitasiDuty[i]);

  JsonArray levelKalib = kalib["level"].to<JsonArray>();
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    JsonObject o = levelKalib.add<JsonObject>();
    o["kosong"] = s.levelKalibKosong[i];
    o["penuh"] = s.levelKalibPenuh[i];
    o["ok"] = s.levelKalibOk[i];
  }
  kalib["level_epoch"] = s.kalibEpoch[6];

  JsonArray suhuKalib = kalib["suhu"].to<JsonArray>();
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    JsonObject o = suhuKalib.add<JsonObject>();
    o["addr"] = s.suhuAddr[i];
    o["offset_x10"] = s.suhuOffsetX10[i];
  }
  kalib["suhu_epoch"] = s.kalibEpoch[7];

  JsonObject mq = kalib["mq"].to<JsonObject>();
  mq["normal"] = s.mqNormal;
  mq["kotor"] = s.mqKotor;
  mq["aktif"] = s.mqAktif;
  kalib["mq_epoch"] = s.kalibEpoch[8];

  doc["heap"] = s.heapFree;
  doc["heap_min"] = s.heapMin;

  serializeJson(doc, out, n);
}

void apiKirimStatus() {
  static char buf[4096];
  apiBuatStatusJson(buf, sizeof(buf));
  Serial.println(buf);
}

void apiKirimEvents() {
  Serial.print(F("{\"ok\":true,\"cmd\":\"events\",\"count\":"));
  Serial.print(eventCount);
  Serial.print(F(",\"events\":["));
  int mulai = (eventCount < EVENT_MAX) ? 0 : eventHead;
  for (int n = 0; n < eventCount; n++) {
    int idx = (mulai + n) % EVENT_MAX;
    if (n) Serial.print(',');
    Serial.print(F("{\"t\":"));
    Serial.print(eventLog[idx].ms);
    Serial.print(F(",\"epoch\":"));
    Serial.print(eventLog[idx].epoch);
    Serial.print(F(",\"kode\":"));
    Serial.print(eventLog[idx].kode);
    Serial.print(F(",\"detail\":\""));
    Serial.print(eventLog[idx].detail);
    Serial.print(F("\"}"));
  }
  Serial.println(F("]}"));
}

void apiBuatPresetsJson(char* out, size_t n) {
  if (out == nullptr || n == 0) return;
  JsonDocument doc;
  doc["ok"] = true;
  doc["cmd"] = "presets";
  doc["count"] = jumlahPreset;
  JsonArray arr = doc["presets"].to<JsonArray>();
  for (int i = 0; i < jumlahPreset; i++) {
    Preset &p = daftarPreset[i];
    JsonObject o = arr.add<JsonObject>();
    o["idx"] = i;
    o["nama"] = p.nama;
    o["pcbPanjang"] = p.pcbPanjang;
    o["pcbLebar"] = p.pcbLebar;
    o["modeRasio"] = p.modeRasio;
    o["persenHCl"] = p.persenHCl;
    o["persenH2O2"] = p.persenH2O2;
    o["persenH2O"] = p.persenH2O;
    o["mlHCl"] = p.mlHCl;
    o["mlH2O2"] = p.mlH2O2;
    o["mlH2O"] = p.mlH2O;
    o["volume"] = volumeTotal(p);
    o["isiTembagaPct"] = p.isiTembagaPct;
    o["tebalTembagaUm"] = p.tebalTembagaUm;
    o["lajuEtchX10"] = p.lajuEtchX10;
    o["modeWaktu"] = p.modeWaktu;
    o["agitasiDetik"] = p.agitasiDetik;
    o["agitasiSpeed"] = p.agitasiSpeed;
    o["siklus"] = p.siklus;
    o["bilasSabun"] = p.bilasSabun;
    o["bilasAir"] = p.bilasAir;
    o["bilasMl"] = p.bilasMl;
    o["bilasDetik"] = p.bilasDetik;
    o["konfirmasiSiklus"] = p.konfirmasiSiklus;
  }
  serializeJson(doc, out, n);
}

char apiPresetJsonBuf[8192];

void apiKirimPresets() {
  apiBuatPresetsJson(apiPresetJsonBuf, sizeof(apiPresetJsonBuf));
  Serial.println(apiPresetJsonBuf);
}

void apiKirimBantuan() {
  Serial.println(F("{\"ok\":true,\"cmd\":\"help\",\"commands\":["
                   "\"status\",\"events\",\"start [idx]\",\"pause\",\"resume\","
                   "\"cancel\",\"drain\",\"clear\",\"preset <idx>\",\"pcb\",\"presets\","
                   "\"tank <0-4> <ml>\",\"config <json>\",\"version\",\"help\","
                   "\"wifi <ssid> <pass>\",\"wifi default\",\"wifiscan\","
                   "\"mqtt <host> <port> <user> <pass>\","
                   "\"device <id>\",\"topic <base>\",\"net [on|off|save]\"]}"));
}

void apiBool(bool b) { Serial.print(b ? F("true") : F("false")); }

// Implementasi di net.h. Dipanggil lebih dulu agar command jaringan
// (wifi/mqtt/device/topic/net) tidak dianggap command proses.
bool netTanganiBaris(char* baris);

void apiBalas(bool ok, const char* cmd, const char* msg) {
  Serial.print(F("{\"ok\":"));
  apiBool(ok);
  Serial.print(F(",\"cmd\":\""));
  Serial.print(cmd);
  Serial.print(F("\""));
  if (msg != nullptr && msg[0]) {
    Serial.print(F(",\"msg\":\""));
    Serial.print(msg);
    Serial.print('"');
  }
  Serial.println('}');
}

// ---------- command dispatcher ----------
bool perintahProses(PerintahProses cmd, int arg, char* hasil, size_t n) {
  if (hasil == nullptr || n == 0) return false;
  hasil[0] = '\0';

  switch (cmd) {
    case CMD_STATUS:
    case CMD_EVENTS:
    case CMD_HELP:
      return true;

    case CMD_START:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (prosesJalan) {
        snprintf(hasil, n, "proses sedang berjalan");
        return false;
      }
      if (pemulihanTersedia) {
        snprintf(hasil, n, "pemulihan tertunda");
        return false;
      }
      if (volumeWadah > 0) {
        snprintf(hasil, n, "nampan belum kosong");
        return false;
      }
      if (arg >= 0 && arg < jumlahPreset) presetTerpilih = arg;
      prosesMulai();
      if (!prosesJalan) {
        snprintf(hasil, n, "start ditolak");
        return false;
      }
      snprintf(hasil, n, "start %s", daftarPreset[presetTerpilih].nama);
      eventCatat(110, "API START");
      pindahHalaman(HAL_PROSES);
      perluGambar = true;
      return true;

    case CMD_PAUSE:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (!prosesJalan || prosesJeda) {
        snprintf(hasil, n, "tidak bisa pause");
        return false;
      }
      prosesToggleJeda();
      snprintf(hasil, n, "pause");
      eventCatat(111, "API PAUSE");
      return true;

    case CMD_RESUME:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (!prosesJalan || !prosesJeda) {
        snprintf(hasil, n, "tidak bisa resume");
        return false;
      }
      prosesToggleJeda();
      snprintf(hasil, n, "resume");
      eventCatat(112, "API RESUME");
      return true;

    case CMD_CANCEL:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (!prosesJalan) {
        snprintf(hasil, n, "tidak ada proses");
        return false;
      }
      prosesBatal();
      snprintf(hasil, n, "cancel");
      eventCatat(113, "API CANCEL");
      return true;

    case CMD_DRAIN:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (prosesJalan) {
        snprintf(hasil, n, "proses berjalan, gunakan cancel");
        return false;
      }
      if (volumeWadah <= 0) {
        snprintf(hasil, n, "nampan kosong");
        return false;
      }
      mulaiDrainWadah();
      snprintf(hasil, n, "drain");
      eventCatat(114, "API DRAIN");
      pindahHalaman(HAL_PROSES);
      perluGambar = true;
      return true;

    case CMD_CLEAR:
      if (safetyState == SAF_AMAN) {
        snprintf(hasil, n, "tidak ada error");
        return false;
      }
      if (!safetyKonfirmasi()) {
        snprintf(hasil, n, "clear ditolak");
        return false;
      }
      snprintf(hasil, n, "clear");
      eventCatat(115, "API CLEAR");
      return true;

    case CMD_PRESET:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (prosesJalan) {
        snprintf(hasil, n, "proses sedang berjalan");
        return false;
      }
      if (arg < 0 || arg >= jumlahPreset) {
        snprintf(hasil, n, "idx preset tidak valid");
        return false;
      }
      presetTerpilih = arg;
      snprintf(hasil, n, "preset %s", daftarPreset[presetTerpilih].nama);
      eventCatat(117, "API PRESET");
      return true;

    case CMD_PCB:
      if (safetyState != SAF_AMAN) {
        snprintf(hasil, n, "safety aktif");
        return false;
      }
      if (!prosesJalan || prosesFase != FASE_WAIT_PCB) {
        snprintf(hasil, n, "tidak menunggu PCB");
        return false;
      }
      prosesKonfirmasiPcb();
      snprintf(hasil, n, "pcb siap");
      eventCatat(118, "API PCB OK");
      return true;
  }
  return false;
}

long* apiTankPtr(int idx) {
  switch (idx) {
    case 0: return &tankHclMl;
    case 1: return &tankH2o2Ml;
    case 2: return &tankH2oMl;
    case 3: return &tankSabunMl;
    case 4: return &tankLimbahMl;
    default: return nullptr;
  }
}

bool apiSetTank(int idx, long ml, char* hasil, size_t n) {
  if (prosesJalan) {
    snprintf(hasil, n, "proses sedang berjalan");
    return false;
  }
  long* t = apiTankPtr(idx);
  if (t == nullptr) {
    snprintf(hasil, n, "idx tangki 0..4");
    return false;
  }
  long kap = (long)kapasitasTank(idx);
  if (ml < 0 || ml > kap) {
    snprintf(hasil, n, "ml 0..%ld", kap);
    return false;
  }
  *t = ml;
  tankDataAda = true;
  simpanTank();
  snprintf(hasil, n, "tank %d = %ld ml", idx, ml);
  eventCatat(116, "API TANK");
  return true;
}

// Remote config: hanya saat IDLE. Target: tank / setting / preset.
// Contoh:
// {"target":"tank","idx":0,"ml":900}
// {"target":"setting","name":"drainMarginPct","value":25}
// {"target":"preset","idx":0,"fields":{"siklus":2,"agitasiDetik":300}}
bool apiKonfigurasiJson(const char* json, char* hasil, size_t n) {
  if (hasil == nullptr || n == 0) return false;
  if (prosesJalan) {
    snprintf(hasil, n, "proses berjalan");
    return false;
  }
  if (json == nullptr) {
    snprintf(hasil, n, "json kosong");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    snprintf(hasil, n, "json tidak valid");
    return false;
  }

  const char* target = doc["target"] | "";
  if (strcmp(target, "tank") == 0) {
    int idx = doc["idx"] | -1;
    long ml = doc["ml"] | -1L;
    return apiSetTank(idx, ml, hasil, n);
  }

  if (strcmp(target, "setting") == 0) {
    const char* name = doc["name"] | "";
    if (!doc["value"].is<int>()) {
      snprintf(hasil, n, "value harus int");
      return false;
    }
    int v = doc["value"];
    Pengaturan t = pengaturan;
    if (strcmp(name, "tempPeringatanX10") == 0) {
      if (v < 200 || v > 1000) { snprintf(hasil, n, "range 200..1000"); return false; }
      t.tempPeringatanX10 = v;
    } else if (strcmp(name, "tempBahayaX10") == 0) {
      if (v < 300 || v > 1200) { snprintf(hasil, n, "range 300..1200"); return false; }
      t.tempBahayaX10 = v;
    } else if (strcmp(name, "drainMarginPct") == 0) {
      if (v < 0 || v > 100) { snprintf(hasil, n, "range 0..100"); return false; }
      t.drainMarginPct = v;
    } else if (strcmp(name, "levelStabilMs") == 0) {
      if (v < 1000 || v > 60000) { snprintf(hasil, n, "range 1000..60000"); return false; }
      t.levelStabilMs = v;
    } else if (strcmp(name, "mqAktif") == 0) {
      t.mqAktif = v ? 1 : 0;
    } else if (strcmp(name, "iotEnabled") == 0) {
      t.iotEnabled = v ? 1 : 0;
    } else if (strcmp(name, "kapHclMl") == 0) {
      if (v < 100 || v > 5000) { snprintf(hasil, n, "range 100..5000"); return false; }
      t.kapHclMl = v;
    } else if (strcmp(name, "kapH2o2Ml") == 0) {
      if (v < 100 || v > 5000) { snprintf(hasil, n, "range 100..5000"); return false; }
      t.kapH2o2Ml = v;
    } else if (strcmp(name, "kapH2oMl") == 0) {
      if (v < 100 || v > 5000) { snprintf(hasil, n, "range 100..5000"); return false; }
      t.kapH2oMl = v;
    } else if (strcmp(name, "kapSabunMl") == 0) {
      if (v < 100 || v > 5000) { snprintf(hasil, n, "range 100..5000"); return false; }
      t.kapSabunMl = v;
    } else if (strcmp(name, "kapLimbahMl") == 0) {
      if (v < 500 || v > 50000) { snprintf(hasil, n, "range 500..50000"); return false; }
      t.kapLimbahMl = v;
    } else {
      snprintf(hasil, n, "setting tidak dikenal");
      return false;
    }
    if (t.tempPeringatanX10 >= t.tempBahayaX10) {
      snprintf(hasil, n, "waspada harus < bahaya");
      return false;
    }
    pengaturan = t;
    clampTankKeKapasitas();
    simpanSet();
    snprintf(hasil, n, "setting %s = %d", name, v);
    eventCatat(122, "REMOTE SET");
    return true;
  }

  if (strcmp(target, "preset") == 0) {
    int idx = doc["idx"] | -1;
    if (idx < 0 || idx >= jumlahPreset) {
      snprintf(hasil, n, "idx preset tidak valid");
      return false;
    }
    JsonObject f = doc["fields"].as<JsonObject>();
    if (f.isNull()) {
      snprintf(hasil, n, "fields kosong");
      return false;
    }

    Preset p = daftarPreset[idx];
    if (f["nama"].is<const char*>()) {
      const char* nm = f["nama"];
      for (const char* q = nm; *q; q++) {
        if (*q == '"' || *q == '\\') {
          snprintf(hasil, n, "nama mengandung karakter tidak aman");
          return false;
        }
      }
      strncpy(p.nama, nm, sizeof(p.nama) - 1);
      p.nama[sizeof(p.nama) - 1] = '\0';
    }
    if (f["pcbPanjang"].is<int>()) p.pcbPanjang = f["pcbPanjang"];
    if (f["pcbLebar"].is<int>()) p.pcbLebar = f["pcbLebar"];
    if (f["persenHCl"].is<int>()) p.persenHCl = f["persenHCl"];
    if (f["persenH2O2"].is<int>()) p.persenH2O2 = f["persenH2O2"];
    if (f["mlHCl"].is<int>()) p.mlHCl = f["mlHCl"];
    if (f["mlH2O2"].is<int>()) p.mlH2O2 = f["mlH2O2"];
    if (f["modeRasio"].is<int>()) p.modeRasio = f["modeRasio"];
    if (f["agitasiDetik"].is<int>()) p.agitasiDetik = f["agitasiDetik"];
    if (f["agitasiSpeed"].is<int>()) p.agitasiSpeed = f["agitasiSpeed"];
    if (f["siklus"].is<int>()) p.siklus = f["siklus"];
    if (f["bilasSabun"].is<int>()) p.bilasSabun = f["bilasSabun"];
    if (f["bilasAir"].is<int>()) p.bilasAir = f["bilasAir"];
    if (f["bilasMl"].is<int>()) p.bilasMl = f["bilasMl"];
    if (f["bilasDetik"].is<int>()) p.bilasDetik = f["bilasDetik"];
    if (f["konfirmasiSiklus"].is<int>()) p.konfirmasiSiklus = f["konfirmasiSiklus"];
    if (f["isiTembagaPct"].is<int>()) p.isiTembagaPct = f["isiTembagaPct"];
    if (f["tebalTembagaUm"].is<int>()) p.tebalTembagaUm = f["tebalTembagaUm"];
    if (f["lajuEtchX10"].is<int>()) p.lajuEtchX10 = f["lajuEtchX10"];
    if (f["modeWaktu"].is<int>()) p.modeWaktu = f["modeWaktu"];

    int vol = f["volume"] | volumeTotal(p);
    if (p.modeRasio == 0) {
      hitungUlang(p, vol);
    } else {
      p.mlH2O = vol - p.mlHCl - p.mlH2O2;
      if (p.mlH2O < 0) p.mlH2O = 0;
      sinkronPersen(p);
    }

    if (!validasiPreset(p)) {
      snprintf(hasil, n, "preset tidak valid");
      return false;
    }
    daftarPreset[idx] = p;
    simpanPreset();
    snprintf(hasil, n, "preset %d disimpan", idx);
    eventCatat(123, "REMOTE PRESET");
    return true;
  }

  snprintf(hasil, n, "target tank/setting/preset");
  return false;
}

// ---------- transport Serial non-blocking ----------
#define API_BUF_MAX 512
char apiBuf[API_BUF_MAX];
int  apiLen = 0;

void apiInit() {
  apiLen = 0;
  Serial.println(F("[API] Serial commands: help, status, events, presets, start, pause, resume, cancel, drain, clear, config, wifi, wifiscan, mqtt, device, topic, net"));
}

void apiProsesBaris(char* baris) {
  if (netTanganiBaris(baris)) return;

  if (strncmp(baris, "config ", 7) == 0) {
    char msg[48];
    bool ok = apiKonfigurasiJson(baris + 7, msg, sizeof(msg));
    apiBalas(ok, "config", msg);
    return;
  }

  char* cmd = strtok(baris, " ");
  if (cmd == nullptr) return;
  char* a1 = strtok(nullptr, " ");
  char* a2 = strtok(nullptr, " ");

  if (strcmp(cmd, "help") == 0) { apiKirimBantuan(); return; }
  if (strcmp(cmd, "status") == 0) { apiKirimStatus(); return; }
  if (strcmp(cmd, "events") == 0) { apiKirimEvents(); return; }
  if (strcmp(cmd, "presets") == 0) { apiKirimPresets(); return; }
  if (strcmp(cmd, "version") == 0) {
    Serial.print(F("{\"ok\":true,\"cmd\":\"version\",\"ver\":\""));
    Serial.print(FW_VERSI);
    Serial.println(F("\"}"));
    return;
  }
  if (strcmp(cmd, "preset") == 0) {
    if (prosesJalan) {
      apiBalas(false, "preset", "proses sedang berjalan");
      return;
    }
    int idx = a1 ? atoi(a1) : -1;
    if (idx < 0 || idx >= jumlahPreset) {
      apiBalas(false, "preset", "idx preset tidak valid");
      return;
    }
    presetTerpilih = idx;
    apiBalas(true, "preset", daftarPreset[idx].nama);
    return;
  }
  if (strcmp(cmd, "tank") == 0) {
    if (!a1 || !a2) {
      apiBalas(false, "tank", "format: tank <0-4> <ml>");
      return;
    }
    char msg[40];
    bool ok = apiSetTank(atoi(a1), atol(a2), msg, sizeof(msg));
    apiBalas(ok, "tank", msg);
    return;
  }

  PerintahProses p;
  if      (strcmp(cmd, "start") == 0)  p = CMD_START;
  else if (strcmp(cmd, "pause") == 0)  p = CMD_PAUSE;
  else if (strcmp(cmd, "resume") == 0) p = CMD_RESUME;
  else if (strcmp(cmd, "cancel") == 0) p = CMD_CANCEL;
  else if (strcmp(cmd, "drain") == 0)  p = CMD_DRAIN;
  else if (strcmp(cmd, "clear") == 0)  p = CMD_CLEAR;
  else if (strcmp(cmd, "pcb") == 0)    p = CMD_PCB;
  else {
    apiBalas(false, cmd, "perintah tidak dikenal");
    return;
  }

  int arg = a1 ? atoi(a1) : -1;
  char msg[48];
  bool ok = perintahProses(p, arg, msg, sizeof(msg));
  apiBalas(ok, cmd, msg);
}

void apiUpdate() {
  // Batasi jumlah karakter per loop agar tidak mengganggu safety/proses.
  for (int n = 0; n < 64 && Serial.available() > 0; n++) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      apiBuf[apiLen] = '\0';
      if (apiLen > 0) apiProsesBaris(apiBuf);
      apiLen = 0;
    } else if (apiLen < API_BUF_MAX - 1) {
      apiBuf[apiLen++] = c;
    } else {
      apiLen = 0;
      apiBalas(false, "api", "baris terlalu panjang");
    }
  }
}
