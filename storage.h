#pragma once
#include <Preferences.h>
#include "config.h"
#include "dbg.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "motor.h"
#include "dcmotor.h"

// Semua data disimpan sebagai SATU blob (satu key) + versi + checksum.
// v2 (v1.0.0): Preset v2 (siklus/bilas), tanpa totalDetik.
#define FORMAT_VERSI 2
#define FORMAT_VERSI_PRESET_LAMA 1

struct DataSimpan {
  uint32_t versi;
  uint32_t jumlah;
  Preset   p[MAKS_PRESET];
  uint32_t crc;
};

// Layout v1 (v0.6.x) untuk migrasi. JANGAN diubah.
struct PresetV1 {
  char  nama[12];
  int   pcbPanjang, pcbLebar;
  int   persenHCl, persenH2O2, persenH2O;
  int   mlHCl, mlH2O2, mlH2O;
  int   agitasiDetik, agitasiSpeed;
  int   totalDetik;
};
struct DataSimpanV1 {
  uint32_t versi;
  uint32_t jumlah;
  PresetV1 p[MAKS_PRESET];
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

// Preset v1 -> v2. Nilai baru memakai default aman; preset pengguna TIDAK
// direset. agitasiDetik lama 4..10 s semantiknya berubah (kini per siklus),
// jadi nilai < 30 diangkat ke 300 s dan dicatat untuk ditinjau pengguna.
void migrasiPresetV1(const PresetV1& a, Preset& b) {
  memset(&b, 0, sizeof(b));
  memcpy(b.nama, a.nama, sizeof(b.nama));
  b.nama[sizeof(b.nama) - 1] = '\0';
  b.pcbPanjang = a.pcbPanjang;
  b.pcbLebar   = a.pcbLebar;
  b.persenHCl  = a.persenHCl;
  b.persenH2O2 = a.persenH2O2;
  b.persenH2O  = a.persenH2O;
  b.mlHCl      = a.mlHCl;
  b.mlH2O2     = a.mlH2O2;
  b.mlH2O      = a.mlH2O;
  b.agitasiDetik = a.agitasiDetik;
  b.agitasiSpeed = a.agitasiSpeed;
  if (b.agitasiDetik < 30) b.agitasiDetik = 300;   // perlu ditinjau
  b.siklus = 3;
  b.bilasSabun = 1;
  b.bilasAir = 2;
  b.bilasMl = 100;
  b.bilasDetik = 20;
  b.konfirmasiSiklus = 0;
}

void muatPreset() {
  DataSimpan d;
  DataSimpanV1 lama;
  bool ada = false, valid = false, migrasi = false;

  if (prefs.begin(PREF_NS, false)) {
    size_t len = prefs.getBytesLength("cfg");
    ada = (len > 0);

    if (len == sizeof(DataSimpan)) {
      prefs.getBytes("cfg", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc))) &&
              (d.jumlah <= MAKS_PRESET);
      for (uint32_t i = 0; valid && i < d.jumlah; i++)
        if (!validasiPreset(d.p[i])) valid = false;
    } else if (len == sizeof(DataSimpanV1)) {
      prefs.getBytes("cfg", &lama, sizeof(lama));
      bool crcOk = (lama.crc == hitungCrc((const uint8_t*)&lama, sizeof(lama) - sizeof(lama.crc)));
      if (crcOk && lama.versi == FORMAT_VERSI_PRESET_LAMA && lama.jumlah <= MAKS_PRESET) {
        jumlahPreset = lama.jumlah;
        for (uint32_t i = 0; i < lama.jumlah; i++) {
          migrasiPresetV1(lama.p[i], daftarPreset[i]);
          daftarPreset[i].agitasiSpeed = constrain(daftarPreset[i].agitasiSpeed, 1, 3);
        }
        migrasi = true;
        valid = true;
      }
    }
    prefs.end();
  }

  if (valid && !migrasi) {
    jumlahPreset = d.jumlah;
    for (int i = 0; i < jumlahPreset; i++) {
      daftarPreset[i] = d.p[i];
      daftarPreset[i].agitasiSpeed = constrain(daftarPreset[i].agitasiSpeed, 1, 3);
    }
    dbg.print(F("[STORE] Dimuat "));
    dbg.print(jumlahPreset);
    dbg.println(F(" preset"));
  } else if (migrasi) {
    dbg.println(F("[STORE] Migrasi preset v1 -> v2"));
    dbg.println(F("[STORE] agitasiDetik lama < 30 s diangkat ke 300 s, tinjau ulang"));
    simpanPreset();
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
// Versi 2 (v0.6.0): + duty/ml DC, + 3 speed agitasi, hasil ukur 0,1 ml.
// Versi 3 (v0.6.2): PUMP_STEP_PER_REV 6400 -> 3200. Migrasi v2: mlPerRev / 2.
// Versi 4 (v1.0.0): model langkah = a*ml + b dan ml = r*t + c + titik uji.
//   Migrasi v2/v3: a = PUMP_STEP_PER_REV / mlPerRev, b = 0; c = 0.
#define FORMAT_VERSI_POMPA 4

struct DataPompaV3 {
  uint32_t versi;
  int32_t  rpm[3];
  float    mlPerRev[3];
  int32_t  dcDuty[2];
  float    dcMlPerSec[2];
  int32_t  agitDuty[3];
  uint32_t crc;
};

struct DataPompa {
  uint32_t versi;
  int32_t  rpm[3];
  float    stepA[3];
  float    stepB[3];
  int32_t  titikN[3];
  float    titikMl[3][6];
  float    titikStep[3][6];
  int32_t  dcDuty[2];
  float    dcR[2];
  float    dcC[2];
  int32_t  dcTitikN[2];
  float    dcTitikDetik[2][6];
  float    dcTitikMl[2][6];
  int32_t  agitDuty[3];
  uint32_t crc;
};

bool simpanPompa() {
  DataPompa d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_POMPA;
  for (int i = 0; i < 3; i++) {
    d.rpm[i]      = rpmPompaValid(kalibStepperRpm[i]);
    d.stepA[i]    = kalibStepperA[i];
    d.stepB[i]    = kalibStepperB[i];
    d.titikN[i]   = kalibStepperTitikN[i];
    for (int j = 0; j < 6; j++) {
      d.titikMl[i][j]   = kalibStepperTitikMl[i][j];
      d.titikStep[i][j] = kalibStepperTitikStep[i][j];
    }
    d.agitDuty[i] = constrain(agitasiSpdDuty[i], 0, 100);
  }
  for (int i = 0; i < 2; i++) {
    d.dcDuty[i]    = constrain(kalibDutyDC[i], 0, 100);
    d.dcR[i]       = kalibDcMlPerSec[i];
    d.dcC[i]       = kalibDcC[i];
    d.dcTitikN[i]  = kalibDcTitikN[i];
    for (int j = 0; j < 6; j++) {
      d.dcTitikDetik[i][j] = kalibDcTitikDetik[i][j];
      d.dcTitikMl[i][j]    = kalibDcTitikMl[i][j];
    }
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
  DataPompaV3 lama;
  bool valid = false;
  bool migrasi = false;

  if (prefs.begin(PREF_NS, false)) {
    size_t len = prefs.getBytesLength("pompa");
    if (len == sizeof(DataPompa)) {
      prefs.getBytes("pompa", &d, sizeof(d));
      bool crcOk = (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      if (crcOk && d.versi == FORMAT_VERSI_POMPA) {
        valid = true;
        for (int i = 0; valid && i < 3; i++) {
          if (d.rpm[i] < PUMP_RPM_MIN || d.rpm[i] > PUMP_RPM_MAX) valid = false;
          if (d.stepA[i] < 0.0f || d.stepA[i] > 100000.0f) valid = false;
          if (d.titikN[i] < 0 || d.titikN[i] > 6) valid = false;
          if (d.agitDuty[i] < 0 || d.agitDuty[i] > 100) valid = false;
        }
        for (int i = 0; valid && i < 2; i++) {
          if (d.dcDuty[i] < 0 || d.dcDuty[i] > 100) valid = false;
          if (d.dcR[i] < 0.0f || d.dcR[i] > 100.0f) valid = false;
          if (d.dcTitikN[i] < 0 || d.dcTitikN[i] > 6) valid = false;
        }
      }
    } else if (len == sizeof(DataPompaV3)) {
      prefs.getBytes("pompa", &lama, sizeof(lama));
      bool crcOk = (lama.crc == hitungCrc((const uint8_t*)&lama, sizeof(lama) - sizeof(lama.crc)));
      if (crcOk && (lama.versi == 2 || lama.versi == 3)) {
        memset(&d, 0, sizeof(d));
        d.versi = FORMAT_VERSI_POMPA;
        for (int i = 0; i < 3; i++) {
          float mlRev = lama.mlPerRev[i];
          if (lama.versi == 2) mlRev *= 0.5f;   // 6400 -> 3200 langkah/putaran
          d.rpm[i] = lama.rpm[i];
          d.stepA[i] = (mlRev > 0.0f) ? (float)PUMP_STEP_PER_REV / mlRev : 0.0f;
          d.stepB[i] = 0.0f;
          d.titikN[i] = 0;
          d.agitDuty[i] = lama.agitDuty[i];
        }
        for (int i = 0; i < 2; i++) {
          d.dcDuty[i] = lama.dcDuty[i];
          d.dcR[i] = lama.dcMlPerSec[i];
          d.dcC[i] = 0.0f;
          d.dcTitikN[i] = 0;
        }
        migrasi = true;
        valid = true;
      }
    }
    prefs.end();
  }

  static const int agitDefault[3] = { AGITASI_SPD1_DEFAULT,
                                      AGITASI_SPD2_DEFAULT,
                                      AGITASI_SPD3_DEFAULT };
  for (int i = 0; i < 3; i++) {
    kalibStepperRpm[i] = valid ? rpmPompaValid(d.rpm[i]) : PUMP_RPM_DEFAULT;
    kalibStepperA[i]   = valid ? d.stepA[i] : 0.0f;
    kalibStepperB[i]   = valid ? d.stepB[i] : 0.0f;
    kalibStepperTitikN[i] = valid ? constrain((int)d.titikN[i], 0, 6) : 0;
    for (int j = 0; j < 6; j++) {
      kalibStepperTitikMl[i][j]   = valid ? d.titikMl[i][j] : 0.0f;
      kalibStepperTitikStep[i][j] = valid ? d.titikStep[i][j] : 0.0f;
    }
    kalibStepperMl[i] = (kalibStepperA[i] > 0.0f)
                          ? (float)PUMP_STEP_PER_REV / kalibStepperA[i] : 0.0f;
    agitasiSpdDuty[i] = valid ? constrain((int)d.agitDuty[i], 0, 100)
                              : agitDefault[i];
  }
  static const int dcDefault[2] = { 80, 100 };   // sabun 80%, limbah 100%
  for (int i = 0; i < 2; i++) {
    kalibDutyDC[i]   = valid ? constrain((int)d.dcDuty[i], 0, 100) : dcDefault[i];
    kalibDcMlPerSec[i] = valid ? d.dcR[i] : 0.0f;
    kalibDcC[i]        = valid ? d.dcC[i] : 0.0f;
    kalibDcTitikN[i]   = valid ? constrain((int)d.dcTitikN[i], 0, 6) : 0;
    for (int j = 0; j < 6; j++) {
      kalibDcTitikDetik[i][j] = valid ? d.dcTitikDetik[i][j] : 0.0f;
      kalibDcTitikMl[i][j]    = valid ? d.dcTitikMl[i][j] : 0.0f;
    }
    // Model disimpan bersama duty. Setelah boot, anggap model valid untuk
    // duty tersimpan; perubahan duty lewat UI akan menghapus model.
    kalibDcModelDuty[i] = (kalibDcMlPerSec[i] > 0.0f) ? kalibDutyDC[i] : -1;
  }

  dbg.println(valid ? F("[STORE] Kalibrasi motor dimuat") : F("[STORE] Kalibrasi motor belum ada (default)"));

  if (migrasi && valid) {
    dbg.println(F("[STORE] Migrasi kalibrasi pompa v2/v3 -> v4 (model a,b / r,c)"));
    simpanPompa();
  }
}

// ================= KALIBRASI SUHU & UDARA (kal2) =================
#define FORMAT_VERSI_KAL2 1

struct DataKal2 {
  uint32_t versi;
  int32_t  suhuAddr[JUMLAH_SUHU];
  int32_t  suhuOffsetX10[JUMLAH_SUHU];
  int32_t  mqNormal;
  int32_t  mqKotor;
  uint32_t crc;
};

bool simpanKal2() {
  DataKal2 d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_KAL2;
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    d.suhuAddr[i] = kalibSuhuAddr[i];
    d.suhuOffsetX10[i] = (int32_t)lround(suhuOffsetX10[i]);
  }
  d.mqNormal = mq135Normal;
  d.mqKotor  = mq135Kotor;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("kal2", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan kal2: OK") : F("[STORE] Simpan kal2: GAGAL"));
  return ok;
}

void muatKal2() {
  DataKal2 d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("kal2") == sizeof(d)) {
      prefs.getBytes("kal2", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_KAL2) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_SUHU; i++)
        if (d.suhuAddr[i] < 1 || d.suhuAddr[i] > 3) valid = false;
    }
    prefs.end();
  }
  if (valid) {
    for (int i = 0; i < JUMLAH_SUHU; i++) {
      kalibSuhuAddr[i] = d.suhuAddr[i];
      suhuOffsetX10[i] = (float)d.suhuOffsetX10[i];
    }
    mq135Normal = d.mqNormal;
    mq135Kotor  = d.mqKotor;
  }
  dbg.println(valid ? F("[STORE] Kal2 (suhu/udara) dimuat") : F("[STORE] Kal2 belum ada (default)"));
}

// ================= PENGATURAN GLOBAL (set) =================
#define FORMAT_VERSI_SET 1

struct DataSet {
  uint32_t versi;
  int32_t tempPeringatanX10;
  int32_t tempBahayaX10;
  int32_t drainMarginPct;
  int32_t levelStabilMs;
  int32_t mqAktif;
  int32_t iotEnabled;
  uint32_t crc;
};

bool simpanSet() {
  DataSet d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_SET;
  d.tempPeringatanX10 = pengaturan.tempPeringatanX10;
  d.tempBahayaX10     = pengaturan.tempBahayaX10;
  d.drainMarginPct    = pengaturan.drainMarginPct;
  d.levelStabilMs     = pengaturan.levelStabilMs;
  d.mqAktif           = pengaturan.mqAktif;
  d.iotEnabled        = pengaturan.iotEnabled;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("set", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan pengaturan: OK") : F("[STORE] Simpan pengaturan: GAGAL"));
  return ok;
}

void muatSet() {
  DataSet d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("set") == sizeof(d)) {
      prefs.getBytes("set", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_SET) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
    }
    prefs.end();
  }
  if (valid) {
    pengaturan.tempPeringatanX10 = constrain((int)d.tempPeringatanX10, 200, 1000);
    pengaturan.tempBahayaX10     = constrain((int)d.tempBahayaX10, 300, 1200);
    // Jaga urutan: peringatan harus selalu di bawah bahaya.
    if (pengaturan.tempPeringatanX10 >= pengaturan.tempBahayaX10)
      pengaturan.tempPeringatanX10 = pengaturan.tempBahayaX10 - 50;
    pengaturan.drainMarginPct    = constrain((int)d.drainMarginPct, 0, 100);
    pengaturan.levelStabilMs     = constrain((int)d.levelStabilMs, 1000, 60000);
    pengaturan.mqAktif           = d.mqAktif ? 1 : 0;
    pengaturan.iotEnabled        = d.iotEnabled ? 1 : 0;
  }
  dbg.println(valid ? F("[STORE] Pengaturan dimuat") : F("[STORE] Pengaturan belum ada (default)"));
}

// ================= PELACAK TANGKI (tank) =================
#define FORMAT_VERSI_TANK 1

struct DataTank {
  uint32_t versi;
  int32_t hcl, h2o2, h2o, sabun, limbah;
  uint32_t crc;
};

bool simpanTank() {
  DataTank d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_TANK;
  d.hcl = (int32_t)tankHclMl;
  d.h2o2 = (int32_t)tankH2o2Ml;
  d.h2o = (int32_t)tankH2oMl;
  d.sabun = (int32_t)tankSabunMl;
  d.limbah = (int32_t)tankLimbahMl;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("tank", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  return ok;
}

void muatTank() {
  DataTank d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("tank") == sizeof(d)) {
      prefs.getBytes("tank", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_TANK) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
    }
    prefs.end();
  }
  if (valid) {
    tankHclMl    = d.hcl;
    tankH2o2Ml   = d.h2o2;
    tankH2oMl    = d.h2o;
    tankSabunMl  = d.sabun;
    tankLimbahMl = d.limbah;
  }
  dbg.println(valid ? F("[STORE] Data tangki dimuat") : F("[STORE] Data tangki belum ada"));
}

// ================= RUNSTATE (pemulihan setelah mati listrik) =================
// Ditulis saat transisi fase/langkah saja, bukan tiap detik, agar NVS awet.
#define FORMAT_VERSI_RUN 1

struct DataRun {
  uint32_t versi;
  int32_t fase;
  int32_t langkah;
  int32_t siklus;
  int32_t volumeWadah;
  int32_t preset;
  uint32_t crc;
};

bool simpanRunstate(int fase, int langkah, int siklus, int volumeWadah, int preset) {
  DataRun d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_RUN;
  d.fase = fase;
  d.langkah = langkah;
  d.siklus = siklus;
  d.volumeWadah = volumeWadah;
  d.preset = preset;
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("runstate", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  return ok;
}

bool muatRunstate(int &fase, int &langkah, int &siklus, int &volumeWadah, int &preset) {
  DataRun d;
  if (!prefs.begin(PREF_NS, true)) return false;
  bool valid = false;
  if (prefs.getBytesLength("runstate") == sizeof(d)) {
    prefs.getBytes("runstate", &d, sizeof(d));
    valid = (d.versi == FORMAT_VERSI_RUN) &&
            (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc))) &&
            (d.fase > 0 && d.fase != FASE_IDLE && d.fase != FASE_SELESAI);
    if (valid) {
      fase = d.fase;
      langkah = d.langkah;
      siklus = d.siklus;
      volumeWadah = d.volumeWadah;
      preset = d.preset;
    }
  }
  prefs.end();
  return valid;
}

void hapusRunstate() {
  if (prefs.begin(PREF_NS, false)) {
    prefs.remove("runstate");
    prefs.end();
  }
}
