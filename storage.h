#pragma once
#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"   // sensorDeltaPenuh/sensorKalibValid (kalibrasi 0%/100%)
#include "sensor_extra.h"   // langkahPerMlX1000, duty agitasi, offset DS18B20, dll

// Semua preset disimpan sebagai SATU blob (satu key) + versi + checksum.
// Satu tulisan = lebih aman kalau daya putus saat menyimpan.
#define FORMAT_VERSI 1

struct DataSimpan {
  uint32_t versi;
  uint32_t jumlah;
  Preset   p[MAKS_PRESET];
  uint32_t crc;          // harus field terakhir
};

Preferences prefs;

uint32_t hitungCrc(const uint8_t* d, size_t n) {   // FNV-1a 32-bit
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
// v3: tambah kering[] (baseline 0% yang fixed) + kapasitasMl[] (untuk kolom ml).
// Data versi 1/2 otomatis dianggap belum terkalibrasi -> tampilan "uncal".
#define FORMAT_VERSI_AMBANG 3

struct DataAmbang {
  uint32_t versi;
  int32_t  ambang[JUMLAH_SENSOR];
  int32_t  kering[JUMLAH_SENSOR];     // raw saat tangki kosong (fixed)
  int32_t  deltaPenuh[JUMLAH_SENSOR];
  int32_t  kapasitasMl[JUMLAH_SENSOR];
  uint8_t  valid[JUMLAH_SENSOR];      // 1 = 0%/100% sudah dikalibrasi
  uint32_t crc;          // harus field terakhir
};

bool simpanAmbang() {
  DataAmbang d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_AMBANG;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    d.ambang[i]       = ambang[i];
    d.kering[i]       = sensorKering[i];
    d.deltaPenuh[i]   = sensorDeltaPenuh[i];
    d.kapasitasMl[i]  = sensorKapasitasMl[i];
    d.valid[i]        = sensorKalibValid[i] ? 1 : 0;
  }
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("amb", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi level: OK")
                 : F("[STORE] Simpan kalibrasi level: GAGAL"));
  return ok;
}

void muatAmbang() {
  DataAmbang d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("amb") == sizeof(d)) {
      prefs.getBytes("amb", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_AMBANG) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_SENSOR; i++) {
        if (d.ambang[i]      < 0 || d.ambang[i]      > 1023)    valid = false;
        if (d.kering[i]      < 0 || d.kering[i]      > 100000)  valid = false;
        if (d.deltaPenuh[i]  < 0 || d.deltaPenuh[i]  > 100000)  valid = false;
        if (d.kapasitasMl[i] < 0 || d.kapasitasMl[i] > 20000)   valid = false;
        if (d.valid[i] > 1) valid = false;
      }
    }
    prefs.end();
  }
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    ambang[i]          = valid ? d.ambang[i] : 0;
    sensorKering[i]     = valid ? (int)d.kering[i] : 0;
    sensorDeltaPenuh[i] = valid ? (int)d.deltaPenuh[i] : TOUCH_DELTA_PENUH_AWAL;
    if (sensorDeltaPenuh[i] <= 0) sensorDeltaPenuh[i] = TOUCH_DELTA_PENUH_AWAL;
    sensorKapasitasMl[i] = valid ? (int)d.kapasitasMl[i] : 0;
    sensorKalibValid[i] = valid && d.valid[i];
  }
  sensorKalibDimuat = valid;
  dbg.println(valid ? F("[STORE] Kalibrasi level dimuat")
                    : F("[STORE] Kalibrasi level belum ada (tampil uncal)"));
}

// ================= KALIBRASI AKTUATOR & SENSOR TAMBAHAN =================
// Pompa stepper (langkah/ml), pompa DC (ml/detik), duty agitasi, PWM fan,
// MQ-135 (baseline + margin), DS18B20 (offset).
#define FORMAT_VERSI_KALIB 1

struct DataKalib {
  uint32_t versi;
  int32_t  langkahPerMlX1000[3];
  int16_t  pompaDcMlPerDtkX10[2];
  int16_t  agitasiDutyMin;
  int16_t  fanPwmMin;
  int16_t  mq135Baseline;
  int16_t  mq135Margin;
  int16_t  ds18b20OffsetX10;
  uint32_t crc;          // harus field terakhir
};

bool simpanKalib() {
  DataKalib d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KALIB;
  for (int i = 0; i < 3; i++) d.langkahPerMlX1000[i] = langkahPerMlX1000[i];
  for (int i = 0; i < 2; i++) d.pompaDcMlPerDtkX10[i] = pompaDcMlPerDtkX10[i];
  d.agitasiDutyMin    = agitasiDutyMin;
  d.fanPwmMin         = fanPwmMin;
  d.mq135Baseline     = mq135Baseline;
  d.mq135Margin       = mq135Margin;
  d.ds18b20OffsetX10  = ds18b20OffsetX10;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  pompaTerapkanKalibrasi();
  dbg.println(ok ? F("[STORE] Simpan kalibrasi aktuator: OK")
                 : F("[STORE] Simpan kalibrasi aktuator: GAGAL"));
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
      for (int i = 0; valid && i < 3; i++)
        if (d.langkahPerMlX1000[i] < 100 || d.langkahPerMlX1000[i] > 100000) valid = false;
      for (int i = 0; valid && i < 2; i++)
        if (d.pompaDcMlPerDtkX10[i] < 0 || d.pompaDcMlPerDtkX10[i] > 3000) valid = false;
      if (d.agitasiDutyMin   < 0 || d.agitasiDutyMin   > 255) valid = false;
      if (d.fanPwmMin        < 0 || d.fanPwmMin        > 255) valid = false;
      if (d.mq135Baseline    < 0 || d.mq135Baseline    > 4095) valid = false;
      if (d.mq135Margin      < 0 || d.mq135Margin      > 4095) valid = false;
      if (d.ds18b20OffsetX10 < -500 || d.ds18b20OffsetX10 > 500) valid = false;
    }
    prefs.end();
  }

  if (valid) {
    for (int i = 0; i < 3; i++) langkahPerMlX1000[i] = d.langkahPerMlX1000[i];
    for (int i = 0; i < 2; i++) pompaDcMlPerDtkX10[i] = d.pompaDcMlPerDtkX10[i];
    agitasiDutyMin   = d.agitasiDutyMin;
    fanPwmMin        = d.fanPwmMin;
    mq135Baseline    = d.mq135Baseline;
    mq135Margin      = d.mq135Margin;
    ds18b20OffsetX10 = d.ds18b20OffsetX10;
  }
  pompaTerapkanKalibrasi();
  dbg.println(valid ? F("[STORE] Kalibrasi aktuator dimuat")
                    : F("[STORE] Kalibrasi aktuator belum ada (nilai bawaan)"));
}
