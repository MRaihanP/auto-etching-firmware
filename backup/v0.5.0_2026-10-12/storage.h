#pragma once
#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "presetops.h"

#define FORMAT_VERSI 2

struct DataSimpan {
  uint32_t versi;
  uint32_t jumlah;
  int32_t  lastActive;
  Preset   p[MAKS_PRESET];
  uint32_t crc;
};

Preferences prefs;

uint32_t hitungCrc(const uint8_t* d, size_t n) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 16777619UL; }
  return h;
}

bool simpanPreset() {
  DataSimpan d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI;
  d.jumlah = jumlahPreset;
  d.lastActive = presetTerakhirAktif;
  for (int i = 0; i < jumlahPreset; i++) d.p[i] = daftarPreset[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));
  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("cfg", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  return ok;
}

void muatPreset() {
  DataSimpan d;
  bool ada = false, valid = false;
  if (prefs.begin(PREF_NS, false)) {
    size_t len = prefs.getBytesLength("cfg");
    ada = (len > 0);
    if (len == sizeof(d)) {
      prefs.getBytes("cfg", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc))) &&
              (d.jumlah <= MAKS_PRESET);
      for (uint32_t i = 0; valid && i < d.jumlah; i++)
        if (!validasiPreset(d.p[i])) valid = false;
    }
    prefs.end();
  }
  if (valid) {
    jumlahPreset = d.jumlah;
    presetTerakhirAktif = constrain((int)d.lastActive, 0, jumlahPreset - 1);
    for (int i = 0; i < jumlahPreset; i++) daftarPreset[i] = d.p[i];
    Serial.print(F("[STORE] Dimuat "));
    Serial.print(jumlahPreset);
    Serial.print(F(" preset, last_active="));
    Serial.println(presetTerakhirAktif);
  } else {
    resetKeDefault();
    presetTerakhirAktif = 0;
    Serial.println(ada ? F("[STORE] Data korup -> default")
                       : F("[STORE] Data belum ada -> default"));
    simpanPreset();
  }
}

// ================= KALIBRASI (sensor + aktuator) =================
#define FORMAT_VERSI_KALIB 2

struct DataKalib {
  uint32_t versi;
  KalibrasiSensor sensor[JUMLAH_SENSOR];
  uint8_t  sensorValid[JUMLAH_SENSOR];
  KalibrasiData aktuator;
  uint32_t crc;
};

bool simpanKalibrasi() {
  DataKalib d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    d.sensor[i] = kalibrasiSensor[i];
    d.sensorValid[i] = kalibrasiSensorValid[i] ? 1 : 0;
  }
  d.aktuator = kalibrasi;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));
  bool ok = false;
  if (prefs.begin(PREF_CALIB, false)) {
    ok = (prefs.putBytes("kalib", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  Serial.println(ok ? F("[STORE] Kalibrasi: OK") : F("[STORE] Kalibrasi: GAGAL"));
  return ok;
}

void muatKalibrasi() {
  DataKalib d;
  bool valid = false;
  if (prefs.begin(PREF_CALIB, false)) {
    if (prefs.getBytesLength("kalib") == sizeof(d)) {
      prefs.getBytes("kalib", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
    }
    prefs.end();
  }
  
  // Kalibrasi sensor
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    if (valid && d.sensorValid[i]) {
      kalibrasiSensor[i] = d.sensor[i];
      kalibrasiSensorValid[i] = true;
    } else {
      memset(&kalibrasiSensor[i], 0, sizeof(KalibrasiSensor));
      kalibrasiSensor[i].kapasitasMl = 500;
      kalibrasiSensor[i].titik[0] = 900;
      kalibrasiSensor[i].titik[1] = 700;
      kalibrasiSensor[i].titik[2] = 500;
      kalibrasiSensor[i].titik[3] = 300;
      kalibrasiSensor[i].titik[4] = 100;
      kalibrasiSensorValid[i] = false;
    }
  }
  
  // Kalibrasi aktuator
  if (valid) {
    kalibrasi = d.aktuator;
  } else {
    kalibrasi.stepperMlPerStepX1000[0] = 25;  // 0.025 ml/step
    kalibrasi.stepperMlPerStepX1000[1] = 25;
    kalibrasi.stepperMlPerStepX1000[2] = 25;
    kalibrasi.mq135Baseline = 180;
    kalibrasi.mq135Margin = 30;
    kalibrasi.ds18b20OffsetX10 = 0;
    kalibrasi.agitasiDutyMin = 50;
  }
  
  Serial.println(valid ? F("[STORE] Kalibrasi dimuat")
                       : F("[STORE] Kalibrasi belum ada (pakai default)"));
}