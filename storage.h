#pragma once
#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "sensor_extra.h"   // dsOffsetX10[], mq135Baseline, mq135Margin

// Semua preset disimpan sebagai SATU blob (satu key) + versi + checksum.
#define FORMAT_VERSI 1

struct DataSimpan {
  uint32_t versi;
  uint32_t jumlah;
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
  d.versi  = FORMAT_VERSI;
  d.jumlah = jumlahPreset;
  for (int i = 0; i < jumlahPreset; i++) d.p[i] = daftarPreset[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("cfg", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.print(F("[STORE] Simpan "));
  dbg.print(jumlahPreset);
  dbg.println(ok ? F(" preset: OK") : F(" preset: GAGAL"));
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
    for (int i = 0; i < jumlahPreset; i++) daftarPreset[i] = d.p[i];
    dbg.print(F("[STORE] Dimuat "));
    dbg.print(jumlahPreset);
    dbg.println(F(" preset"));
  } else {
    resetKeDefault();
    dbg.println(ada ? F("[STORE] Data korup/versi beda -> default")
                       : F("[STORE] Data belum ada -> default"));
    simpanPreset();
  }
}

// ================= KALIBRASI SENSOR LEVEL =================
#define FORMAT_VERSI_KALIB 1

struct DataKalib {
  uint32_t versi;
  int32_t  kosong[JUMLAH_SENSOR];
  int32_t  penuh[JUMLAH_SENSOR];
  uint32_t crc;
};

bool simpanKalib() {
  DataKalib d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    d.kosong[i] = sensorKalibKosong[i];
    d.penuh[i]  = sensorKalibPenuh[i];
  }
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi: OK") : F("[STORE] Simpan kalibrasi: GAGAL"));
  return ok;
}

void muatKalib() {
  DataKalib d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal") == sizeof(d)) {
      prefs.getBytes("kal", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_SENSOR; i++)
        if (d.kosong[i] <= 0 || d.penuh[i] <= 0 || d.penuh[i] <= d.kosong[i]) valid = false;
    }
    prefs.end();
  }
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    sensorKalibKosong[i] = valid ? d.kosong[i] : 113000;
    sensorKalibPenuh[i]  = valid ? d.penuh[i]  : 156000;
  }
  dbg.println(valid ? F("[STORE] Kalibrasi dimuat") : F("[STORE] Kalibrasi belum ada (default)"));
}

// ================= KALIBRASI SENSOR SUHU & UDARA =================
// Blob TERPISAH dari kalibrasi level ("kal"): kalau struct ini ikut berubah,
// kalibrasi 0%/100% yang sudah disimpan di "kal" tidak jadi hilang.
#define FORMAT_VERSI_KALIB2 1

struct DataKalib2 {
  uint32_t versi;
  int16_t  dsOffsetX10[JUMLAH_KANAL_SUHU];   // x10 degC per kanal
  int16_t  mq135Baseline;
  int16_t  mq135Margin;
  uint32_t crc;                // harus field terakhir
};

bool simpanKalib2() {
  DataKalib2 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB2;
  for (int i = 0; i < JUMLAH_KANAL_SUHU; i++) d.dsOffsetX10[i] = dsOffsetX10[i];
  d.mq135Baseline    = mq135Baseline;
  d.mq135Margin      = mq135Margin;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal2", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi suhu/udara: OK")
                 : F("[STORE] Simpan kalibrasi suhu/udara: GAGAL"));
  return ok;
}

void muatKalib2() {
  DataKalib2 d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal2") == sizeof(d)) {
      prefs.getBytes("kal2", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB2) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_KANAL_SUHU; i++)
        if (d.dsOffsetX10[i] < -500 || d.dsOffsetX10[i] > 500) valid = false;
      if (d.mq135Baseline < 0 || d.mq135Baseline > 4095) valid = false;
      if (d.mq135Margin   < 0 || d.mq135Margin   > 4095) valid = false;
    }
    prefs.end();
  }
  if (valid) {
    for (int i = 0; i < JUMLAH_KANAL_SUHU; i++) dsOffsetX10[i] = d.dsOffsetX10[i];
    mq135Baseline    = d.mq135Baseline;
    mq135Margin      = d.mq135Margin;
  }
  dbg.println(valid ? F("[STORE] Kalibrasi suhu/udara dimuat")
                    : F("[STORE] Kalibrasi suhu/udara belum ada (default)"));
}

// ================= KALIBRASI STEPPER (ml per rotasi) =================
// Blob TERPISAH: "kal3" - simpan kalibStepperMl[3] agar langkahPerMl otomatis terhitung
#define FORMAT_VERSI_KALIB3 1

struct DataKalib3 {
  uint32_t versi;
  float    stepperMl[3];   // ml per rotasi untuk HCl, H2O2, Air
  uint32_t crc;
};

bool simpanKalibStepper() {
  DataKalib3 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB3;
  for (int i = 0; i < 3; i++) d.stepperMl[i] = kalibStepperMl[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal3", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi stepper: OK")
                 : F("[STORE] Simpan kalibrasi stepper: GAGAL"));
  return ok;
}

void muatKalibStepper() {
  DataKalib3 d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal3") == sizeof(d)) {
      prefs.getBytes("kal3", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KALIB3) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < 3; i++)
        if (d.stepperMl[i] < 0.1 || d.stepperMl[i] > 10.0) valid = false;
    }
    prefs.end();
  }
  for (int i = 0; i < 3; i++) {
    kalibStepperMl[i] = valid ? d.stepperMl[i] : 1.0;  // default 1.0 ml/rotasi
  }
  dbg.println(valid ? F("[STORE] Kalibrasi stepper dimuat")
                    : F("[STORE] Kalibrasi stepper belum ada (default)"));
}
