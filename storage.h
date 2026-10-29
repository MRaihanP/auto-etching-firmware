#pragma once
#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "motor.h"

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
    for (int i = 0; i < jumlahPreset; i++) {
      daftarPreset[i] = d.p[i];
      // v0.6.0: speed agitasi hanya 1-3; preset lama (4/5) di-clamp.
      daftarPreset[i].agitasiSpeed = constrain(daftarPreset[i].agitasiSpeed, 1, 3);
    }
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

// ================= KALIBRASI POMPA & MOTOR =================
// Key terpisah dari "kal" agar kalibrasi sensor level lama tetap valid.
// Versi 2 (v0.6.0): + duty/ml DC, + 3 speed agitasi, hasil ukur 0,1 ml.
// Versi 3 (v0.6.2): PUMP_STEP_PER_REV 6400 -> 3200 (DRV8825 1/16). Data v2
//   dimigrasi otomatis: mlPerRev dibagi 2 (lihat muatPompa()).
#define FORMAT_VERSI_POMPA 3

struct DataPompa {
  uint32_t versi;
  int32_t  rpm[3];
  float    mlPerRev[3];
  int32_t  dcDuty[2];
  float    dcMlPerSec[2];
  int32_t  agitDuty[3];
  uint32_t crc;
};

bool simpanPompa() {
  DataPompa d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_POMPA;
  for (int i = 0; i < 3; i++) {
    d.rpm[i]      = rpmPompaValid(kalibStepperRpm[i]);
    d.mlPerRev[i] = kalibStepperMl[i];
    d.agitDuty[i] = constrain(agitasiSpdDuty[i], 0, 100);
  }
  for (int i = 0; i < 2; i++) {
    d.dcDuty[i]    = constrain(kalibDutyDC[i], 0, 100);
    d.dcMlPerSec[i] = kalibDcMlPerSec[i];
  }
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("pompa", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kalibrasi motor: OK") : F("[STORE] Simpan kalibrasi motor: GAGAL"));
  return ok;
}

void muatPompa() {
  DataPompa d;
  bool valid = false;
  bool migrasi = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("pompa") == sizeof(d)) {
      prefs.getBytes("pompa", &d, sizeof(d));
      bool crcOk = (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      if (crcOk && (d.versi == FORMAT_VERSI_POMPA || d.versi == 2)) {
        valid = true;
        // Migrasi v2 -> v3: data v2 dikalibrasi saat firmware memakai 6400
        // langkah/putaran, padahal hardware DRV8825 di 1/16 (3200). Perintah
        // "10 rotasi" saat itu = 20 rotasi fisik, jadi mlPerRev tersimpan 2x
        // terlalu besar. Bagi 2 supaya volume dosing tetap benar.
        if (d.versi == 2) {
          for (int i = 0; i < 3; i++) d.mlPerRev[i] *= 0.5f;
          migrasi = true;
        }
        for (int i = 0; valid && i < 3; i++) {
          if (d.rpm[i] < PUMP_RPM_MIN || d.rpm[i] > PUMP_RPM_MAX) valid = false;
          if (d.mlPerRev[i] < 0.0f || d.mlPerRev[i] > 100.0f) valid = false;
          if (d.agitDuty[i] < 0 || d.agitDuty[i] > 100) valid = false;
        }
        for (int i = 0; valid && i < 2; i++) {
          if (d.dcDuty[i] < 0 || d.dcDuty[i] > 100) valid = false;
          if (d.dcMlPerSec[i] < 0.0f || d.dcMlPerSec[i] > 100.0f) valid = false;
        }
      }
    }
    prefs.end();
  }

  static const int agitDefault[3] = { AGITASI_SPD1_DEFAULT,
                                      AGITASI_SPD2_DEFAULT,
                                      AGITASI_SPD3_DEFAULT };
  for (int i = 0; i < 3; i++) {
    kalibStepperRpm[i] = valid ? rpmPompaValid(d.rpm[i]) : PUMP_RPM_DEFAULT;
    kalibStepperMl[i]  = valid ? d.mlPerRev[i] : 0.0f;
    kalibStepperHasilMl10[i] =
        (int)lround(kalibStepperMl[i] * (float)PUMP_KALIB_REV * 10.0f);
    agitasiSpdDuty[i] = valid ? constrain((int)d.agitDuty[i], 0, 100)
                              : agitDefault[i];
  }
  static const int dcDefault[2] = { 80, 100 };   // sabun 80%, limbah 100%
  for (int i = 0; i < 2; i++) {
    kalibDutyDC[i]    = valid ? constrain((int)d.dcDuty[i], 0, 100) : dcDefault[i];
    kalibDcMlPerSec[i] = valid ? d.dcMlPerSec[i] : 0.0f;
    kalibDcHasilMl[i]  = (int)lround(kalibDcMlPerSec[i] * (float)KALIB_DC_DETIK);
  }

  dbg.println(valid ? F("[STORE] Kalibrasi motor dimuat") : F("[STORE] Kalibrasi motor belum ada (default)"));

  if (migrasi && valid) {
    dbg.println(F("[STORE] Migrasi kalibrasi pompa v2 -> v3 (mlPerRev / 2)"));
    simpanPompa();
  }
}
