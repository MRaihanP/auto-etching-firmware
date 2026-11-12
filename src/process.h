#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"
#include "presetops.h"
#include "motor.h"
#include "dcmotor.h"
#include "buzzer.h"
#include "sensor.h"
#include "safety.h"
#include "storage.h"

// ================= MESIN PROSES v1.0.1 =================
// Alur:
//   IDLE -> PRECHECK -> WAIT_PCB -> [SIKLUS PENUH xN] -> SELESAI
// Satu siklus penuh:
//   DOSE_AIR -> DOSE_H2O2 -> DOSE_HCL -> AGITASI -> DRAIN
//   -> BILAS SABUN xS -> BILAS AIR xM -> DECIDE (siklus berikutnya)
// Semua bilas dihitung PER SIKLUS; `siklus` di preset = jumlah pengulangan
// penuh dosing + etching + bilas.
//
// Semua aksi aktuator non-blocking. Fase ditentukan kondisi nyata (pompa
// benar-benar berhenti, timer habis, sensor), bukan persentase waktu.

const char* namaFase[] = {
  "IDLE", "PRECHECK", "WAIT PCB", "ETCH", "BILAS SABUN",
  "BILAS AIR", "DRAIN", "PEMULIHAN", "SELESAI"
};
const char* namaLangkah[] = {
  "-", "DOSE AIR", "DOSE H2O2", "DOSE HCL", "AGITASI", "DRAIN",
  "DECIDE", "KONFIRMASI", "ISI", "AGITASI BILAS"
};

bool prosesJalan = false;          // true = proses aktif (termasuk saat jeda)
bool prosesJeda  = false;
bool dialogBatal = false;          // tawaran "Drain wadah?" sedang tampil
bool dialogBatalMinta = false;     // dialog "Batalkan proses?" sedang tampil
bool prosesTungguPcb = false;
bool prosesKonfirmasi = false;     // layar "trace sudah OK?"
int  konfirmasiSel = 0;            // 0 = ulangi siklus, 1 = selesai & bilas
bool prosesPeringatanSuhu = false;
bool prosesPeringatanLevel = false;   // sumber kering saat proses (warning saja)
bool prosesPeringatanLimbah = false;  // limbah mendekati penuh (warning saja)
bool pemulihanAktif = false;
bool pemulihanTersedia = false;
int  pemulihanTahap = 0;
int  pemulihanFase = 0, pemulihanLangkah = 0, pemulihanSiklus = 0, pemulihanVolume = 0, pemulihanPreset = 0;

unsigned long prosesAkumMs      = 0;
unsigned long prosesTerakhirMs  = 0;
unsigned long prosesTotalMsEst  = 0;
unsigned long langkahMulaiMs    = 0;
unsigned long langkahDurasiMs   = 0;
unsigned long langkahBatasMs    = 0;
unsigned long jedaMulaiMs       = 0;
int  prosesDetikTerakhir        = -1;
FaseProses prosesFase           = FASE_IDLE;
LangkahProses prosesLangkah     = LANGKAH_NONE;
int  prosesSiklus               = 1;
int  prosesBilasSabunKe         = 0;
int  prosesBilasAirKe           = 0;
int  volumeWadah                = 0;
bool suhuWarnSudah              = false;

// Evakuasi darurat: E-stop/ERROR menyalakan pompa limbah sampai cairan
// tersedot. Berjalan di loop() terpisah dari prosesJalan.
bool          evakuasiAktif    = false;
unsigned long evakuasiMulaiMs  = 0;
unsigned long evakuasiDurasiMs = 0;
int           evakuasiMl       = 0;

// ---------- util tangki ----------
void tankKurangi(int idx, int ml) {
  if (ml <= 0) return;
  long* t = NULL;
  if (idx == 0) t = &tankHclMl;
  else if (idx == 1) t = &tankH2o2Ml;
  else if (idx == 2) t = &tankH2oMl;
  else if (idx == 3) t = &tankSabunMl;
  if (!t) return;
  *t -= ml;
  if (*t < 0) *t = 0;
}

// Limbah bertambah setiap drain. Nilai dibatasi kapasitas agar tidak
// melebihi angka yang bisa ditampilkan/dipakai precheck.
void tankTambahLimbah(int ml) {
  if (ml <= 0) return;
  tankLimbahMl += ml;
  long kap = (long)pengaturan.kapLimbahMl;
  if (tankLimbahMl > kap) tankLimbahMl = kap;
}

// Durasi drain dari volume ledger + estimasi residu. Durasi utama mengikuti
// volume; purge tetap ditambahkan oleh dcDrainMs().
unsigned long drainMsVolume(int ml) {
  if (ml <= 0) return 0;
  return dcDrainMs(ml + DRAIN_RESIDU_ML);
}

const char* namaSumberLevel(int i);

// Estimasi volume dari sensor level. Sensor hanya titik, jadi ini perkiraan
// untuk bootstrap/migrasi ketika ledger belum diisi atau masih 0.
bool estimasiTankDariSensor(int idx, long* hasil) {
  if (idx < 0 || idx > 4 || hasil == nullptr) return false;
  if (levelFault(idx)) return false;
  // Hanya percaya persen bila sensor sudah dikalibrasi user.
  if (!sensorKalibSimpan[idx]) return false;
  // Pakai persen langsung; status basah bisa tertinggal debounce saat boot.
  int pct = sensorPersen[idx];
  if (pct < LEVEL_ON_PCT) return false;
  long kap = (long)kapasitasTank(idx);
  if (kap <= 0) return false;
  long est = kap * (long)pct / 100L;
  if (est < TANK_PERINGATAN_ML) est = TANK_PERINGATAN_ML;
  if (est > kap) est = kap;
  *hasil = est;
  return true;
}

// Isi ledger yang masih 0 dari sensor. Contoh: setelah upgrade firmware,
// tangki HCl fisik berisi 90% tetapi tankHclMl masih 0 karena data lama
// tidak pernah mencatat volume.
void sinkronTankDariSensor() {
  long* t[5] = { &tankHclMl, &tankH2o2Ml, &tankH2oMl,
                 &tankSabunMl, &tankLimbahMl };
  bool ada = false;
  for (int i = 0; i < 5; i++) {
    if (*t[i] > 0) continue;
    long est = 0;
    if (!estimasiTankDariSensor(i, &est)) continue;
    *t[i] = est;
    ada = true;
    dbg.print(F("[TANK] Sinkron "));
    dbg.print(namaSumberLevel(i));
    dbg.print(F(" = "));
    dbg.print(est);
    dbg.print(F(" ml (sensor "));
    dbg.print(sensorPersen[i]);
    dbg.println(F("%)"));
  }
  if (ada) {
    tankDataAda = true;
    simpanTank();
    eventCatat(106, "TANK SYNC");
  }
}

// Sinkronisasi berkelanjutan saat alat IDLE. Ini membuat isi ulang tangki
// manual (tanpa menu apa pun) langsung tercermin di ledger dan status web.
// Tidak berjalan saat proses/jeda/WAIT_PCB/pemulihan/evakuasi atau safety aktif.
void sinkronTankOtomatisDariSensor() {
  static unsigned long terakhir = 0;
  if (millis() - terakhir < 2000UL) return;
  terakhir = millis();

  if (prosesJalan || prosesJeda || prosesTungguPcb || pemulihanTersedia ||
      evakuasiAktif || safetyState != SAF_AMAN || halaman == HAL_TANK_EDIT) {
    return;
  }

  long* t[5] = { &tankHclMl, &tankH2o2Ml, &tankH2oMl,
                 &tankSabunMl, &tankLimbahMl };
  bool berubah = false;
  for (int i = 0; i < 5; i++) {
    if (!sensorKalibSimpan[i] || levelFault(i)) continue;
    long kap = (long)kapasitasTank(i);
    if (kap <= 0) continue;

    int pct = constrain(sensorPersen[i], 0, 100);
    long est = kap * (long)pct / 100L;
    long beda = est - *t[i];
    if (beda < 0) beda = -beda;
    long ambang = kap * 3L / 100L;
    if (ambang < 30L) ambang = 30L;
    if (beda < ambang) continue;

    *t[i] = est;
    berubah = true;
    dbg.print(F("[TANK] Auto sinkron "));
    dbg.print(namaSumberLevel(i));
    dbg.print(F(" = "));
    dbg.print(est);
    dbg.print(F(" ml (sensor "));
    dbg.print(pct);
    dbg.println(F("%)"));
  }

  if (berubah) {
    tankDataAda = true;
    simpanTank();
    eventCatat(106, "TANK AUTO");
  }
}

// Peringatan pasca-precheck: sumber kering/limbah hampir penuh hanya
// menandai UI, tidak menghentikan proses.
void setPeringatanLevel(int idx, bool kering) {
  if (kering && !prosesPeringatanLevel) {
    prosesPeringatanLevel = true;
    buzzerWarn();
    dbg.print(F("[PROSES] Peringatan level sumber "));
    dbg.println(idx);
  }
}

int prosesVolumeKebutuhan() {
  Preset &p = daftarPreset[presetTerpilih];
  long perSiklus = (long)volumeTotal(p) +
                   (long)(p.bilasSabun + p.bilasAir) * p.bilasMl;
  return (int)((long)p.siklus * perSiklus);
}

// Peringatan "tangki hampir habis" untuk layar proses. Nilai 0 pada tangki
// sumber berarti pelacak belum diisi, jadi tidak dianggap habis.
bool prosesTangkiMenipis() {
  if (tankHclMl   > 0 && tankHclMl   < TANK_PERINGATAN_ML) return true;
  if (tankH2o2Ml  > 0 && tankH2o2Ml  < TANK_PERINGATAN_ML) return true;
  if (tankH2oMl   > 0 && tankH2oMl   < TANK_PERINGATAN_ML) return true;
  if (tankSabunMl > 0 && tankSabunMl < TANK_PERINGATAN_ML) return true;
  if (tankLimbahMl > (long)pengaturan.kapLimbahMl - TANK_PERINGATAN_ML) return true;
  return false;
}

// ---------- estimasi waktu ----------
unsigned long prosesEstimasiLangkahMs(LangkahProses l, bool bilasSabun = false) {
  Preset &p = daftarPreset[presetTerpilih];
  switch (l) {
    case L_DOSE_AIR:  return pompaPerkiraanMs(2, p.mlH2O);
    case L_DOSE_H2O2: return pompaPerkiraanMs(1, p.mlH2O2);
    case L_DOSE_HCL:  return pompaPerkiraanMs(0, p.mlHCl);
    case L_BILAS_AGITASI: return (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
    default: return 0;
  }
}

int estimasiTotalDetikPreset(const Preset &p) {
  unsigned long ms = 0;
  for (int s = 0; s < p.siklus; s++) {
    // Dosing + etching + drain etchant
    ms += pompaPerkiraanMs(2, p.mlH2O);
    ms += pompaPerkiraanMs(1, p.mlH2O2);
    ms += pompaPerkiraanMs(0, p.mlHCl);
    ms += (unsigned long)SKALA_UJI_CEPAT(agitasiDetikEfektif(p)) * 1000UL;
    ms += drainMsVolume(volumeTotal(p));
    // Bilas sabun + air PER SIKLUS
    for (int i = 0; i < p.bilasSabun; i++) {
      ms += dcDosisMs(0, p.bilasMl);
      ms += (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
      ms += drainMsVolume(p.bilasMl);
    }
    for (int i = 0; i < p.bilasAir; i++) {
      ms += pompaPerkiraanMs(2, p.bilasMl);
      ms += (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
      ms += drainMsVolume(p.bilasMl);
    }
  }
  return (int)(ms / 1000UL);
}

int prosesTotalDetik() {
  return estimasiTotalDetikPreset(daftarPreset[presetTerpilih]);
}

bool prosesSelesai() { return prosesFase == FASE_SELESAI; }
bool prosesDrainBatalAktif() { return prosesJalan && prosesFase == FASE_DRAIN_BATAL; }

// ---------- info dosing untuk layar proses ----------
// true bila sedang mengisi cairan. `nama` = bahan, `targetMl` = target,
// `sudahMl` = perkiraan ml yang sudah dituangkan.
// Stepper memakai posisi nyata (pompaProgres); pompa sabun DC memakai
// rasio waktu terpakai terhadap durasi perintah.
bool prosesInfoDosing(const char** nama, int* targetMl, int* sudahMl) {
  Preset &p = daftarPreset[presetTerpilih];
  const char* nm = nullptr;
  int target = 0;
  bool berbasisWaktu = false;

  if (prosesFase == FASE_ETCH) {
    if (prosesLangkah == L_DOSE_AIR)       { nm = "AIR";  target = p.mlH2O; }
    else if (prosesLangkah == L_DOSE_H2O2) { nm = "H2O2"; target = p.mlH2O2; }
    else if (prosesLangkah == L_DOSE_HCL)  { nm = "HCl";  target = p.mlHCl; }
  } else if ((prosesFase == FASE_BILAS_AIR ||
              prosesFase == FASE_PEMULIHAN) && prosesLangkah == L_ISI) {
    nm = "AIR"; target = p.bilasMl;
  } else if (prosesFase == FASE_BILAS_SABUN && prosesLangkah == L_ISI) {
    nm = "SABUN"; target = p.bilasMl;
    berbasisWaktu = true;
  }

  if (nm == nullptr || target <= 0) return false;

  int sudah = 0;
  if (berbasisWaktu) {
    if (langkahDurasiMs > 0) {
      unsigned long now = prosesJeda ? jedaMulaiMs : millis();
      unsigned long lewat = now - langkahMulaiMs;
      if (lewat > langkahDurasiMs) lewat = langkahDurasiMs;
      sudah = (int)((long)target * (long)lewat / (long)langkahDurasiMs);
    }
  } else {
    int idx = (prosesLangkah == L_DOSE_HCL) ? 0 :
              (prosesLangkah == L_DOSE_H2O2) ? 1 : 2;
    sudah = (int)((long)target * pompaProgres(idx) / 100);
  }

  *nama = nm;
  *targetMl = target;
  *sudahMl = sudah;
  return true;
}

// ---------- simpan status pemulihan ----------
void simpanRunstateSekarang() {
  simpanRunstate((int)prosesFase, (int)prosesLangkah,
                 prosesSiklus, volumeWadah, presetTerpilih);
}

// Catat volume tepat setelah cairan masuk, bukan hanya saat transisi langkah.
void tambahVolumeWadah(int ml) {
  if (ml <= 0) return;
  volumeWadah += ml;
  simpanRunstateSekarang();
}

void kosongkanVolumeWadah() {
  volumeWadah = 0;
  simpanRunstateSekarang();
}

// Saat cancel/E-stop di tengah dosing, kurangi volume target yang belum
// benar-benar dipompa. Dengan begitu drain memakai volume nyata di nampan.
void sesuaikanVolumeDosisAktual() {
  const char* nama = nullptr;
  int target = 0, sudah = 0;
  if (!prosesInfoDosing(&nama, &target, &sudah)) return;
  int sisa = target - sudah;
  if (sisa <= 0) return;
  if (sisa > volumeWadah) sisa = volumeWadah;
  volumeWadah -= sisa;
  simpanRunstateSekarang();
  dbg.print(F("[PROSES] Koreksi dosis "));
  dbg.print(nama);
  dbg.print(F(": -"));
  dbg.print(sisa);
  dbg.print(F(" ml, nampan "));
  dbg.print(volumeWadah);
  dbg.println(F(" ml"));
}

// Sisa cairan saat drain sedang berjalan, dihitung dari waktu yang sudah
// berjalan dibanding durasi drain. Dipakai bila cancel/E-stop di tengah drain
// agar drain berikutnya tidak mengulang volume penuh.
int sisaVolumeDrainAktual() {
  if (prosesLangkah != L_DRAIN || volumeWadah <= 0 || langkahDurasiMs == 0)
    return volumeWadah;
  unsigned long now = prosesJeda ? jedaMulaiMs : millis();
  unsigned long lewat = now - langkahMulaiMs;
  if (lewat >= langkahDurasiMs) return 0;
  long sisa = (long)volumeWadah -
              ((long)volumeWadah * (long)lewat / (long)langkahDurasiMs);
  return (int)constrain(sisa, 0L, (long)volumeWadah);
}

// Cancel/E-stop: sesuaikan volume dengan progres dosis atau drain yang aktif.
void sesuaikanVolumeAktual() {
  if (prosesLangkah == L_DRAIN) {
    int sisa = sisaVolumeDrainAktual();
    int terpakai = volumeWadah - sisa;
    if (terpakai > 0) tankTambahLimbah(terpakai);
    volumeWadah = sisa;
    if (tankDataAda) simpanTank();
    simpanRunstateSekarang();
    dbg.print(F("[PROSES] Koreksi drain, sisa "));
    dbg.print(volumeWadah);
    dbg.println(F(" ml"));
  } else {
    sesuaikanVolumeDosisAktual();
  }
}

// ---------- transisi langkah ----------
void mulaiLangkah(LangkahProses l, unsigned long durasiMs = 0,
                  unsigned long timeoutMs = 0) {
  prosesLangkah = l;
  langkahMulaiMs = millis();
  langkahDurasiMs = durasiMs;
  langkahBatasMs = (timeoutMs > 0) ? (millis() + timeoutMs) : 0;
  simpanRunstateSekarang();
  perluGambar = true;
  dbg.print(F("[PROSES] "));
  dbg.print(namaFase[prosesFase]);
  dbg.print(F(" / "));
  dbg.print(namaLangkah[l]);
  dbg.print(F(" siklus "));
  dbg.println(prosesSiklus);
}

bool langkahLewat() {
  if (langkahDurasiMs == 0) return false;
  return (millis() - langkahMulaiMs) >= langkahDurasiMs;
}

bool langkahTimeout() {
  return langkahBatasMs != 0 && (long)(millis() - langkahBatasMs) >= 0;
}

void cekSuhuProses() {
  float t = sensorSuhuTertinggi();
  if (t <= -100.0f) return;
  if (t >= pengaturan.tempBahayaX10 / 10.0f) {
    char d[48];
    snprintf(d, sizeof(d), "Suhu %.1f C > %.1f C", t,
             pengaturan.tempBahayaX10 / 10.0f);
    safetyRaiseKonteks(2, d, "Dinginkan / cek sensor");
    return;
  }
  if (t >= pengaturan.tempPeringatanX10 / 10.0f) {
    if (!suhuWarnSudah) {
      suhuWarnSudah = true;
      prosesPeringatanSuhu = true;
      buzzerWarn();
      perluGambar = true;
      dbg.print(F("[PROSES] Peringatan suhu "));
      dbg.println(t);
    }
  } else {
    prosesPeringatanSuhu = false;
  }
}

// ---------- precheck ----------
const char* namaSumberLevel(int i) {
  static const char* nama[5] = { "HCl", "H2O2", "H2O", "SABUN", "LIMBAH" };
  return (i >= 0 && i < 5) ? nama[i] : "?";
}

// Pemeriksaan sensor saat proses berjalan. Setelah PRECHECK lulus, level
// kering/hampir penuh hanya menjadi peringatan; fault sensor tetap E03.
bool cekLevelSumber(int i) {
  if (levelFault(i)) {
    char d[48];
    snprintf(d, sizeof(d), "Sensor %s FAULT (raw %d)",
             namaSumberLevel(i), sensorRaw[i]);
    safetyRaiseKonteks(3, d, "Cek wiring/kalibrasi sensor");
    return false;
  }
  if (levelKering(i)) setPeringatanLevel(i, true);
  return true;
}

bool cekLevelLimbah() {
  if (levelFault(4)) {
    char d[48];
    snprintf(d, sizeof(d), "Sensor LIMBAH FAULT (raw %d)", sensorRaw[4]);
    safetyRaiseKonteks(3, d, "Cek wiring/kalibrasi sensor");
    return false;
  }
  // Saat pompa drain baru menyala, cairan bisa memercik ke strip sensor.
  if (prosesLangkah == L_DRAIN &&
      (millis() - langkahMulaiMs) < LIMBAH_DRAIN_GRACE_MS) return true;

  // Jaring pengaman terakhir: hanya level sangat tinggi yang menghentikan
  // proses. Level 85% hanya menjadi peringatan.
  if (levelLimbahKritis()) {
    char d[48];
    snprintf(d, sizeof(d), "Limbah kritis %d%%, ruang habis", sensorPersen[4]);
    safetyRaiseKonteks(6, d, "Kosongkan tangki limbah");
    return false;
  }
  if (levelLimbahPenuh()) {
    if (!prosesPeringatanLimbah) {
      prosesPeringatanLimbah = true;
      buzzerWarn();
      dbg.print(F("[PROSES] Peringatan limbah "));
      dbg.print(sensorPersen[4]);
      dbg.println(F("%"));
    }
  }
  return true;
}

bool precheckLulus() {
  Preset &p = daftarPreset[presetTerpilih];
  char d[48];
  clampTankKeKapasitas();
  sinkronTankDariSensor();

  // Nampan harus kosong dan tidak ada pemulihan tertunda.
  if (volumeWadah > 0) {
    snprintf(d, sizeof(d), "Nampan masih %d ml", volumeWadah);
    safetyRaiseKonteks(5, d, "Drain wadah dulu");
    return false;
  }
  if (pemulihanTersedia) {
    safetyRaiseKonteks(8, "Pemulihan belum diselesaikan", "Pilih drain/batalkan");
    return false;
  }
  // Kalibrasi: sebutkan pompa yang bermasalah.
  for (int i = 0; i < 3; i++) {
    if (!pompaSiap(i)) {
      snprintf(d, sizeof(d), "Pompa %s belum dikalibrasi", namaSumberLevel(i));
      safetyRaiseKonteks(9, d, "Kalibrasi Motor");
      return false;
    }
  }
  if (p.bilasSabun > 0 && kalibDcMlPerSec[0] <= 0.0f) {
    safetyRaiseKonteks(9, "Pompa SABUN belum dikalibrasi", "Kalibrasi Motor");
    return false;
  }
  if (!sensorSuhuValid(2)) {
    safetyRaiseKonteks(3, "Sensor suhu chamber tidak valid", "Cek sensor DS18B20");
    return false;
  }

  // Sensor level sumber.
  int sumberAktif = (p.bilasSabun > 0) ? 4 : 3;
  for (int i = 0; i < sumberAktif; i++) {
    if (levelFault(i)) {
      snprintf(d, sizeof(d), "Sensor %s FAULT (raw %d)",
               namaSumberLevel(i), sensorRaw[i]);
      safetyRaiseKonteks(3, d, "Cek wiring/kalibrasi sensor");
      return false;
    }
    if (levelKering(i)) {
      snprintf(d, sizeof(d), "Sensor %s kering, isi jerigen", namaSumberLevel(i));
      safetyRaiseKonteks(5, d, "Isi + menu Tangki");
      return false;
    }
  }
  if (levelFault(4)) {
    safetyRaiseKonteks(3, "Sensor LIMBAH FAULT", "Cek wiring/kalibrasi sensor");
    return false;
  }
  if (levelLimbahPenuh()) {
    snprintf(d, sizeof(d), "Limbah sudah penuh (%d%%)", sensorPersen[4]);
    safetyRaiseKonteks(6, d, "Kosongkan tangki limbah");
    return false;
  }

  // Kebutuhan seluruh proses vs sisa ledger tiap jerigen.
  long butuhHcl   = (long)p.siklus * p.mlHCl;
  long butuhH2O2  = (long)p.siklus * p.mlH2O2;
  long butuhH2O   = (long)p.siklus *
                    ((long)p.mlH2O + (long)p.bilasAir * p.bilasMl);
  long butuhSabun = (long)p.siklus * p.bilasSabun * p.bilasMl;

  if (butuhHcl > (long)pengaturan.kapHclMl) {
    snprintf(d, sizeof(d), "HCl butuh %ld > kap %d", butuhHcl, pengaturan.kapHclMl);
    safetyRaiseKonteks(5, d, "Kurangi siklus/volume");
    return false;
  }
  if (tankHclMl < butuhHcl) {
    snprintf(d, sizeof(d), "HCl butuh %ld, sisa %ld", butuhHcl, tankHclMl);
    safetyRaiseKonteks(5, d, "Isi + menu Tangki");
    return false;
  }
  if (butuhH2O2 > (long)pengaturan.kapH2o2Ml) {
    snprintf(d, sizeof(d), "H2O2 butuh %ld > kap %d", butuhH2O2, pengaturan.kapH2o2Ml);
    safetyRaiseKonteks(5, d, "Kurangi siklus/volume");
    return false;
  }
  if (tankH2o2Ml < butuhH2O2) {
    snprintf(d, sizeof(d), "H2O2 butuh %ld, sisa %ld", butuhH2O2, tankH2o2Ml);
    safetyRaiseKonteks(5, d, "Isi + menu Tangki");
    return false;
  }
  if (butuhH2O > (long)pengaturan.kapH2oMl) {
    snprintf(d, sizeof(d), "H2O butuh %ld > kap %d", butuhH2O, pengaturan.kapH2oMl);
    safetyRaiseKonteks(5, d, "Kurangi siklus/bilas");
    return false;
  }
  if (tankH2oMl < butuhH2O) {
    snprintf(d, sizeof(d), "H2O butuh %ld, sisa %ld", butuhH2O, tankH2oMl);
    safetyRaiseKonteks(5, d, "Isi + menu Tangki");
    return false;
  }
  if (butuhSabun > (long)pengaturan.kapSabunMl) {
    snprintf(d, sizeof(d), "SABUN butuh %ld > kap %d", butuhSabun, pengaturan.kapSabunMl);
    safetyRaiseKonteks(5, d, "Kurangi bilas/siklus");
    return false;
  }
  if (tankSabunMl < butuhSabun) {
    snprintf(d, sizeof(d), "SABUN butuh %ld, sisa %ld", butuhSabun, tankSabunMl);
    safetyRaiseKonteks(5, d, "Isi + menu Tangki");
    return false;
  }

  // Muat tembaga vs kapasitas larutan: peringatan dini, bukan blokir.
  float cuGram = tembagaGram(p);
  float cuKapasitas = (float)volumeTotal(p) * KAPASITAS_CU_G_PER_L / 1000.0f;
  if (cuGram > cuKapasitas) {
    dbg.print(F("[PROSES] Peringatan: tembaga "));
    dbg.print(cuGram);
    dbg.print(F(" g > kapasitas larutan "));
    dbg.print(cuKapasitas);
    dbg.print(F(" g, minimal kimia "));
    dbg.print(kimiaMinMl(p));
    dbg.println(F(" ml, perbesar volume"));
  }

  // Kapasitas limbah: total cairan + estimasi residu tiap drain.
  long totalCair = (long)p.siklus *
                   (volumeTotal(p) +
                    (long)(p.bilasSabun + p.bilasAir) * p.bilasMl);
  long residu = (long)p.siklus * (1 + p.bilasSabun + p.bilasAir) *
                DRAIN_RESIDU_ML;
  long ruang = (long)pengaturan.kapLimbahMl - tankLimbahMl;
  if (totalCair + residu > ruang) {
    snprintf(d, sizeof(d), "Limbah perlu %ld, ruang %ld", totalCair + residu, ruang);
    safetyRaiseKonteks(6, d, "Kosongkan tangki limbah");
    return false;
  }
  return true;
}

// ---------- siklus ----------
// Satu siklus penuh = dosing + etching + drain + bilas sabun/air.
void mulaiSiklus();
void mulaiBilasSiklus();
void mulaiBilasBerikut();
void selesaiSiklus();

void prosesSelesaiFinal() {
  prosesFase = FASE_SELESAI;
  prosesLangkah = LANGKAH_NONE;
  agitasiSet(0);
  pompaLimbahSet(false);
  pompaSabunSet(false);
  pompaStopAll();
  buzzerSelesai();
  hapusRunstate();
  simpanTank();
  eventCatat(101, "SELESAI");
  perluGambar = true;
  dbg.println(F("[PROSES] Selesai"));
}

// Mulai isi bilas sabun (pompa DC). Volume dicatat saat mulai agar cancel
// di tengah pengisian tetap tahu wadah sudah berisi.
void mulaiIsiSabun() {
  Preset &p = daftarPreset[presetTerpilih];
  unsigned long d = dcDosisMs(0, p.bilasMl);
  if (d == 0) {
    safetyRaiseKonteks(9, "Pompa SABUN durasi 0", "Kalibrasi pompa sabun");
    return;
  }
  mulaiLangkah(L_ISI, d, d + PROSES_TIMEOUT_MARGIN_MS);
  pompaSabunSet(true);
  tambahVolumeWadah(p.bilasMl);
  tankKurangi(3, p.bilasMl);
}

void mulaiIsiAir() {
  Preset &p = daftarPreset[presetTerpilih];
  unsigned long est = pompaPerkiraanMs(2, p.bilasMl);
  mulaiLangkah(L_ISI, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
  if (!pompaMulai(2, p.bilasMl)) {
    safetyRaiseKonteks(9, "Pompa AIR gagal start", "Cek selang/prime");
  } else {
    tambahVolumeWadah(p.bilasMl);
    tankKurangi(2, p.bilasMl);
  }
}

void mulaiSiklus() {
  Preset &p = daftarPreset[presetTerpilih];
  prosesFase = FASE_ETCH;
  prosesSiklus = constrain(prosesSiklus, 1, p.siklus);
  prosesBilasSabunKe = 0;
  prosesBilasAirKe   = 0;
  unsigned long est = pompaPerkiraanMs(2, p.mlH2O);
  mulaiLangkah(L_DOSE_AIR, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
  if (!pompaMulai(2, p.mlH2O)) {
    safetyRaiseKonteks(9, "Pompa AIR gagal start", "Cek selang/prime");
  } else {
    // Dicatat saat mulai, bukan saat selesai: bila proses dibatalkan di
    // tengah dosing, volume wadah tetap diperhitungkan untuk drain.
    tambahVolumeWadah(p.mlH2O);
    tankKurangi(2, p.mlH2O);
  }
  perluGambar = true;
}

// Mulai tahap bilas untuk siklus yang baru selesai di-etch.
void mulaiBilasSiklus() {
  Preset &p = daftarPreset[presetTerpilih];
  if (p.bilasSabun > 0) {
    prosesFase = FASE_BILAS_SABUN;
    prosesBilasSabunKe = 1;
    prosesBilasAirKe   = 0;
    mulaiIsiSabun();
  } else if (p.bilasAir > 0) {
    prosesFase = FASE_BILAS_AIR;
    prosesBilasAirKe   = 1;
    prosesBilasSabunKe = 0;
    mulaiIsiAir();
  } else {
    selesaiSiklus();
  }
}

// Lanjut ke bilas berikutnya setelah satu bilas selesai di-drain.
void mulaiBilasBerikut() {
  Preset &p = daftarPreset[presetTerpilih];
  if (prosesFase == FASE_BILAS_SABUN) {
    if (prosesBilasSabunKe < p.bilasSabun) {
      prosesBilasSabunKe++;
      mulaiIsiSabun();
      return;
    }
    if (p.bilasAir > 0) {
      prosesFase = FASE_BILAS_AIR;
      prosesBilasAirKe = 1;
      mulaiIsiAir();
      return;
    }
  } else if (prosesFase == FASE_BILAS_AIR) {
    if (prosesBilasAirKe < p.bilasAir) {
      prosesBilasAirKe++;
      mulaiIsiAir();
      return;
    }
  }
  selesaiSiklus();
}

// Satu siklus penuh (termasuk bilas) selesai. Bila masih ada siklus,
// lanjut ke dosing berikutnya; bila konfirmasi aktif, tunggu pilihan user.
void selesaiSiklus() {
  Preset &p = daftarPreset[presetTerpilih];
  if (prosesSiklus < p.siklus) {
    if (p.konfirmasiSiklus) {
      prosesKonfirmasi = true;
      konfirmasiSel = 0;
      mulaiLangkah(L_KONFIRMASI);
    } else {
      prosesSiklus++;
      mulaiSiklus();
    }
    return;
  }
  // Siklus terakhir juga ditanyakan bila konfirmasi aktif, supaya pengguna
  // bisa mengulang siklus terakhir sebelum benar-benar selesai.
  if (p.konfirmasiSiklus) {
    prosesKonfirmasi = true;
    konfirmasiSel = 0;
    mulaiLangkah(L_KONFIRMASI);
    return;
  }
  prosesSelesaiFinal();
}

// ---------- api publik ----------
void prosesMulai() {
  if (otaAktif) return;   // jangan jalankan aktuator saat update firmware
  if (!safetyBolehAktuator()) {
    // UI menampilkan toast; proses tidak boleh jalan saat E-STOP/ERROR.
    return;
  }
  if (volumeWadah > 0) {
    // Wadah masih berisi sisa proses sebelumnya. Jangan reset pelacak volume:
    // pengguna harus melakukan drain lebih dulu.
    dbg.print(F("[PROSES] Wadah belum kosong ("));
    dbg.print(volumeWadah);
    dbg.println(F(" ml), drain dulu"));
    return;
  }
  Preset &p = daftarPreset[presetTerpilih];
  prosesJalan = true;
  prosesJeda = false;
  dialogBatal = false;
  dialogBatalMinta = false;
  prosesTungguPcb = false;
  prosesKonfirmasi = false;
  konfirmasiSel = 0;
  prosesPeringatanSuhu = false;
  prosesPeringatanLevel = false;
  prosesPeringatanLimbah = false;
  suhuWarnSudah = false;
  pemulihanAktif = false;
  prosesAkumMs = 0;
  prosesTerakhirMs = millis();
  prosesDetikTerakhir = -1;
  prosesSiklus = 1;
  prosesBilasSabunKe = 0;
  prosesBilasAirKe = 0;
  volumeWadah = 0;
  prosesTotalMsEst = (unsigned long)prosesTotalDetik() * 1000UL;
  prosesFase = FASE_PRECHECK;
  prosesLangkah = LANGKAH_NONE;
  langkahBatasMs = 0;
  simpanRunstateSekarang();

  dbg.print(F("[PROSES] Mulai: "));
  dbg.print(p.nama);
  dbg.print(F(" ("));
  dbg.print(p.siklus);
  dbg.print(F(" siklus, est "));
  dbg.print(prosesTotalDetik());
  dbg.println(F(" s)"));
  buzzerMulai();
  eventCatat(100, p.nama);
  perluGambar = true;
}

void prosesKonfirmasiPcb() {
  if (!prosesJalan || prosesFase != FASE_WAIT_PCB) return;
  prosesTungguPcb = false;
  prosesSiklus = 1;
  mulaiSiklus();
}

void prosesKonfirmasiSiklus(bool ulangi) {
  if (!prosesKonfirmasi) return;
  prosesKonfirmasi = false;
  Preset &p = daftarPreset[presetTerpilih];
  if (!ulangi) {
    // Siklus penuh (termasuk bilas) sudah selesai -> akhiri proses.
    prosesSelesaiFinal();
  } else {
    // Ulangi siklus. Pada siklus terakhir, ulangi siklus yang sama.
    if (prosesSiklus < p.siklus) prosesSiklus++;
    mulaiSiklus();
  }
  perluGambar = true;
}

void prosesToggleJeda() {
  if (!prosesJalan || prosesSelesai()) return;
  if (prosesFase == FASE_PRECHECK || prosesFase == FASE_WAIT_PCB) return;
  if (prosesKonfirmasi) return;
  if (!safetyBolehAktuator()) return;

  prosesJeda = !prosesJeda;
  if (prosesJeda) {
    pompaJedaSemua();
    dcJedaSemua();
    jedaMulaiMs = millis();
    jedaSel = 0;
    buzzerJeda();
    dbg.println(F("[PROSES] Jeda (semua aktuator)"));
  } else {
    unsigned long delta = millis() - jedaMulaiMs;
    langkahMulaiMs += delta;
    if (langkahBatasMs) langkahBatasMs += delta;
    pompaLanjutSemua();
    dcLanjutSemua();
    jedaSel = 0;
    buzzerLanjut();
    dbg.println(F("[PROSES] Lanjut"));
  }
  perluGambar = true;
}

// Mulai drain wadah yang sedang berisi (dipakai cancel dan sisa proses).
void mulaiDrainWadah() {
  if (volumeWadah <= 0) return;
  unsigned long ms = drainMsVolume(volumeWadah);
  if (ms == 0) {
    char d[48];
    snprintf(d, sizeof(d), "Drain %d ml gagal: pompa limbah", volumeWadah);
    safetyRaiseKonteks(9, d, "Kalibrasi pompa limbah");
    return;
  }
  prosesJalan = true;
  prosesJeda = false;
  prosesFase = FASE_DRAIN_BATAL;
  prosesLangkah = L_DRAIN;
  prosesAkumMs = 0;
  prosesTotalMsEst = ms;
  prosesTerakhirMs = millis();
  mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
  pompaLimbahSet(true);
  eventCatat(104, "DRAIN");
  dbg.print(F("[PROSES] Drain wadah "));
  dbg.print(volumeWadah);
  dbg.println(F(" ml"));
}

// E-stop/ERROR: semua motor mati, lalu pompa limbah menyedot cairan wadah.
// Durasi memakai model limbah atau fallback waktu bila belum dikalibrasi.
void mulaiEvakuasiDarurat() {
  if (evakuasiAktif) return;
  if (volumeWadah <= 0) return;
  // Jangan menyalakan pompa bila sensor menyatakan tangki limbah penuh atau
  // ruang ledger tidak cukup.
  if (safetyKode == 6 || levelLimbahKritis() ||
      tankLimbahMl + volumeWadah + DRAIN_RESIDU_ML > (long)pengaturan.kapLimbahMl) {
    dbg.println(F("[EVAKUASI] Dibatalkan: tangki limbah penuh"));
    safetySalin(safetyAksi, sizeof(safetyAksi), "Kosongkan limbah lalu drain");
    return;
  }
  unsigned long ms = drainMsVolume(volumeWadah);
  if (ms == 0) return;

  evakuasiMl       = volumeWadah;
  evakuasiMulaiMs  = millis();
  evakuasiDurasiMs = ms;
  evakuasiAktif    = true;

  pompaStopAllDarurat();
  dcStopAll();
  float duty = (kalibDutyDC[1] > 0) ? (float)kalibDutyDC[1] : 100.0f;
  dcJedaAktif = false;
  dcSetPct(DC_LIMBAH, duty);

  dbg.print(F("[EVAKUASI] Pompa limbah ON, volume "));
  dbg.print(evakuasiMl);
  dbg.print(F(" ml, durasi "));
  dbg.print(ms / 1000UL);
  dbg.println(F(" s"));
  perluGambar = true;
}

int evakuasiSisaDetik() {
  if (!evakuasiAktif) return 0;
  unsigned long lewat = millis() - evakuasiMulaiMs;
  if (lewat >= evakuasiDurasiMs) return 0;
  return (int)((evakuasiDurasiMs - lewat + 999UL) / 1000UL);
}

// Panggil tiap loop(), juga saat safetyState != SAF_AMAN.
void evakuasiUpdate() {
  if (!evakuasiAktif) return;

  // Selama evakuasi hanya pompa limbah yang boleh berjalan.
  dcStop(DC_AGI);
  dcStop(DC_SABUN);

  if (levelLimbahKritis()) {
    dcStop(DC_LIMBAH);
    evakuasiAktif = false;
    dbg.println(F("[EVAKUASI] Stop: tangki limbah kritis"));
    perluGambar = true;
    return;
  }
  if (millis() - evakuasiMulaiMs < evakuasiDurasiMs) {
    float duty = (kalibDutyDC[1] > 0) ? (float)kalibDutyDC[1] : 100.0f;
    if (dcTargetPct[DC_LIMBAH] < 1.0f) dcSetPct(DC_LIMBAH, duty);
    return;
  }

  dcStop(DC_LIMBAH);
  tankTambahLimbah(evakuasiMl + DRAIN_RESIDU_ML);
  volumeWadah  = 0;
  evakuasiMl   = 0;
  evakuasiAktif = false;
  simpanTank();
  hapusRunstate();
  buzzerNotif();
  eventCatat(105, "EVAKUASI");
  dbg.println(F("[EVAKUASI] Selesai"));
  perluGambar = true;
}

// Berhenti total tanpa dialog; aman dipanggil dari safety (sekali).
void prosesBatalDarurat() {
  bool aktif = prosesJalan;
  // E-stop/error adalah stop terkendali, bukan kehilangan daya. Bila wadah
  // masih berisi, simpan runstate agar boot berikutnya tetap menawarkan drain.
  if (aktif && volumeWadah > 0) {
    simpanRunstateSekarang();
  } else if (aktif) {
    hapusRunstate();
  }
  prosesJalan = false;
  prosesJeda = false;
  dialogBatal = false;
  dialogBatalMinta = false;
  prosesTungguPcb = false;
  prosesKonfirmasi = false;
  prosesFase = FASE_IDLE;
  prosesLangkah = LANGKAH_NONE;
  langkahBatasMs = 0;
  pompaStopAllDarurat();
  dcStopAllDarurat();
  dcJedaAktif = false;
  if (aktif) {
    buzzerStopProses();
    // Jangan hapus runstate di sini: bila wadah masih berisi, runstate sudah
    // disimpan di atas agar boot berikutnya menawarkan drain.
    if (tankDataAda) simpanTank();
    // Bila error terjadi saat pemulihan, tawarkan pemulihan lagi setelah
    // pengguna membersihkan error, memakai volume wadah saat ini.
    if (pemulihanAktif) {
      pemulihanAktif = false;
      pemulihanVolume = volumeWadah;
      pemulihanTersedia = true;
    }
    dbg.println(F("[PROSES] Dibatalkan darurat"));
    eventCatat(103, "STOP DARURAT");
  }
  // E-stop/ERROR: nyalakan pompa limbah sampai cairan tersedot.
  if (safetyState != SAF_AMAN) mulaiEvakuasiDarurat();
  perluGambar = true;
}

// Batal oleh pengguna: matikan semua motor lain, lalu LANGSUNG drain wadah.
void prosesBatal() {
  // Catat volume nyata sebelum pompa dihentikan.
  sesuaikanVolumeAktual();
  bool adaIsi = volumeWadah > 0;
  pompaStopAllDarurat();
  dcStopAll();
  dcJedaAktif = false;
  prosesJeda = false;
  prosesTungguPcb = false;
  prosesKonfirmasi = false;
  dialogBatalMinta = false;
  dialogBatal = false;
  prosesLangkah = LANGKAH_NONE;
  buzzerBatal();
  eventCatat(102, "BATAL");

  if (adaIsi && safetyBolehAktuator()) {
    mulaiDrainWadah();
  } else {
    if (adaIsi) simpanRunstateSekarang(); else hapusRunstate();
    prosesJalan = false;
    prosesFase = FASE_IDLE;
    prosesLangkah = LANGKAH_NONE;
    if (tankDataAda) simpanTank();
    dbg.print(F("[PROSES] Batal, volume wadah="));
    dbg.println(volumeWadah);
  }
  perluGambar = true;
}

// ENTER pada dialog "Drain wadah?" untuk sisa proses sebelumnya.
void prosesDrainBatal() {
  dialogBatal = false;
  dialogBatalMinta = false;
  if (volumeWadah <= 0) return;
  if (!safetyBolehAktuator()) {
    perluGambar = true;
    return;
  }
  pompaStopAllDarurat();
  dcStopAll();
  mulaiDrainWadah();
  perluGambar = true;
}

// Dipanggil setelah proses SELESAI dan user keluar dari layar proses.
void prosesKeluarSelesai() {
  prosesJalan = false;
  prosesJeda = false;
  dialogBatal = false;
  dialogBatalMinta = false;
  prosesFase = FASE_IDLE;
  prosesLangkah = LANGKAH_NONE;
  perluGambar = true;
}

// ================= PEMULIHAN SETELAH MATI LISTRIK =================
void prosesSetPemulihan(int fase, int langkah, int siklus, int volume, int preset) {
  pemulihanTersedia = true;
  pemulihanAktif = false;
  pemulihanFase = fase;
  pemulihanLangkah = langkah;
  pemulihanSiklus = siklus;
  pemulihanVolume = volume;
  pemulihanPreset = preset;
  buzzerNotif();
}

// Batalkan total dari layar pemulihan: drain saja tanpa bilas, lalu IDLE.
void prosesBatalPemulihan() {
  if (pemulihanVolume <= 0) {
    volumeWadah = 0;
    pemulihanTersedia = false;
    pemulihanAktif = false;
    prosesJalan = false;
    prosesFase = FASE_IDLE;
    prosesLangkah = LANGKAH_NONE;
    hapusRunstate();
    perluGambar = true;
    return;
  }
  if (!safetyBolehAktuator()) return;
  volumeWadah = constrain(pemulihanVolume, 0, TANGKI_MAKS_ML);
  pemulihanTersedia = false;
  pemulihanAktif = false;
  buzzerBatal();
  mulaiDrainWadah();
  perluGambar = true;
}

void prosesAbaikanPemulihan() {
  // Abaikan = hentikan proses tanpa menjalankan aktuator apa pun. Volume
  // wadah tetap dilacak di RAM agar proses baru di sesi ini meminta drain,
  // tetapi runstate dihapus supaya peringatan resume tidak muncul lagi.
  pompaStopAllDarurat();
  dcStopAll();
  dcJedaAktif = false;
  prosesJalan = false;
  prosesJeda = false;
  prosesTungguPcb = false;
  prosesKonfirmasi = false;
  dialogBatal = false;
  dialogBatalMinta = false;
  prosesFase = FASE_IDLE;
  prosesLangkah = LANGKAH_NONE;
  langkahBatasMs = 0;
  volumeWadah = constrain(pemulihanVolume, 0, TANGKI_MAKS_ML);
  pemulihanTersedia = false;
  pemulihanAktif = false;
  hapusRunstate();
  buzzerStopProses();
  perluGambar = true;
  dbg.println(F("[PROSES] Pemulihan diabaikan, proses dihentikan"));
  eventCatat(107, "ABAIKAN");
}

// Abaikan dari layar jeda: hentikan proses tanpa drain, kembali ke menu.
// Jika wadah masih berisi, runstate disimpan agar boot berikutnya tahu.
void prosesAbaikanJeda() {
  sesuaikanVolumeAktual();
  bool adaIsi = volumeWadah > 0;
  pompaStopAllDarurat();
  dcStopAll();
  dcJedaAktif = false;
  prosesTungguPcb = false;
  prosesKonfirmasi = false;
  dialogBatal = false;
  dialogBatalMinta = false;

  if (adaIsi) simpanRunstateSekarang(); else hapusRunstate();
  prosesJalan = false;
  prosesJeda = false;
  prosesFase = FASE_IDLE;
  prosesLangkah = LANGKAH_NONE;
  langkahBatasMs = 0;
  if (tankDataAda) simpanTank();
  buzzerStopProses();
  perluGambar = true;
  dbg.println(F("[PROSES] Jeda diabaikan, proses dihentikan"));
  eventCatat(107, "ABAIKAN");
}

// Drain isi wadah, lalu satu bilas air + agitasi singkat + drain.
void prosesMulaiPemulihan(bool drainBilas) {
  if (!drainBilas) { prosesAbaikanPemulihan(); return; }
  if (!safetyBolehAktuator()) return;
  prosesJalan = true;
  prosesJeda = false;
  dialogBatal = false;
  dialogBatalMinta = false;
  pemulihanAktif = true;
  pemulihanTersedia = false;
  pemulihanTahap = 1;
  eventCatat(108, "PEMULIHAN");
  prosesFase = FASE_PEMULIHAN;
  prosesSiklus = pemulihanSiklus > 0 ? pemulihanSiklus : 1;
  prosesAkumMs = 0;
  prosesTerakhirMs = millis();
  volumeWadah = constrain(pemulihanVolume, 0, TANGKI_MAKS_ML);
  prosesTotalMsEst = 0;

  unsigned long ms = drainMsVolume(volumeWadah);
  if (volumeWadah > 0) {
    if (ms == 0) {
      // Ada isi wadah tetapi durasi drain tak bisa dihitung. Jangan lewati
      // drain: hentikan dan minta pengguna melengkapi kalibrasi pompa DC.
      dbg.println(F("[PROSES] Pemulihan ditolak: kalibrasi DC belum lengkap"));
      pemulihanAktif = false;
      pemulihanTersedia = true;   // tawarkan pemulihan lagi setelah error clear
      safetyRaiseKonteks(9, "Drain pemulihan tanpa model", "Kalibrasi pompa limbah");
      return;
    }
    mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
    pompaLimbahSet(true);
  } else {
    // Tidak ada isi: langsung ke bilas air.
    Preset &p = daftarPreset[presetTerpilih];
    volumeWadah = 0;
    pemulihanTahap = 3;
    unsigned long est = pompaPerkiraanMs(2, p.bilasMl);
    mulaiLangkah(L_ISI, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
    if (!pompaMulai(2, p.bilasMl)) {
      pemulihanAktif = false;
      pemulihanTersedia = true;
      safetyRaiseKonteks(9, "Pompa AIR gagal start", "Cek selang/prime");
    } else {
      tambahVolumeWadah(p.bilasMl);
      tankKurangi(2, p.bilasMl);
    }
  }
  perluGambar = true;
}

// ================= UPDATE =================
void prosesUpdate() {
  if (!prosesJalan) return;
  if (!safetyBolehAktuator()) {
    prosesBatalDarurat();
    return;
  }

  unsigned long now = millis();
  // Waktu tunggu PCB (dan precheck) tidak dihitung sebagai progres proses.
  bool faseHitung = (prosesFase != FASE_PRECHECK && prosesFase != FASE_WAIT_PCB);
  if (!prosesJeda && faseHitung) prosesAkumMs += now - prosesTerakhirMs;
  prosesTerakhirMs = now;

  int d = prosesAkumMs / 1000;
  if (d != prosesDetikTerakhir) {
    prosesDetikTerakhir = d;
    perluGambar = true;
  }

  if (prosesJeda) return;

  cekSuhuProses();
  if (!safetyBolehAktuator()) return;

  if (langkahTimeout()) {
    char d[48];
    snprintf(d, sizeof(d), "Timeout %s siklus %d",
             namaLangkah[prosesLangkah], prosesSiklus);
    dbg.println(F("[PROSES] Timeout"));
    safetyRaiseKonteks(4, d, "Cek pompa/selang");
    return;
  }

  Preset &p = daftarPreset[presetTerpilih];

  switch (prosesFase) {
    case FASE_PRECHECK:
      if (precheckLulus()) {
        prosesFase = FASE_WAIT_PCB;
        prosesTungguPcb = true;
        prosesLangkah = LANGKAH_NONE;
        langkahBatasMs = 0;
        simpanRunstateSekarang();
        perluGambar = true;
        dbg.println(F("[PROSES] Precheck OK, menunggu PCB"));
      }
      break;

    case FASE_WAIT_PCB:
      // Menunggu ENTER pengguna (prosesKonfirmasiPcb).
      break;

    case FASE_ETCH:
      switch (prosesLangkah) {
        case L_DOSE_AIR:
          if (pompaAktif(2) && !cekLevelSumber(2)) break;
          if (pompaSelesai(2)) {
            unsigned long est = pompaPerkiraanMs(1, p.mlH2O2);
            mulaiLangkah(L_DOSE_H2O2, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
            if (!pompaMulai(1, p.mlH2O2)) {
              safetyRaiseKonteks(9, "Pompa H2O2 gagal start", "Cek selang/prime");
            } else {
              tambahVolumeWadah(p.mlH2O2);
              tankKurangi(1, p.mlH2O2);
            }
          }
          break;
        case L_DOSE_H2O2:
          if (pompaAktif(1) && !cekLevelSumber(1)) break;
          if (pompaSelesai(1)) {
            unsigned long est = pompaPerkiraanMs(0, p.mlHCl);
            mulaiLangkah(L_DOSE_HCL, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
            if (!pompaMulai(0, p.mlHCl)) {
              safetyRaiseKonteks(9, "Pompa HCl gagal start", "Cek selang/prime");
            } else {
              tambahVolumeWadah(p.mlHCl);
              tankKurangi(0, p.mlHCl);
            }
          }
          break;
        case L_DOSE_HCL:
          if (pompaAktif(0) && !cekLevelSumber(0)) break;
          if (pompaSelesai(0)) {
            agitasiSet(p.agitasiSpeed);
            unsigned long durasi =
                (unsigned long)SKALA_UJI_CEPAT(agitasiDetikEfektif(p)) * 1000UL;
            mulaiLangkah(L_AGITASI, durasi, durasi + 10000UL);
          }
          break;
        case L_AGITASI:
          if (langkahLewat()) {
            agitasiSet(0);
            if (volumeWadah > 0) {
              unsigned long ms = drainMsVolume(volumeWadah);
              if (ms == 0) { safetyRaiseKonteks(9, "Durasi drain 0 ms", "Cek model pompa limbah"); break; }
              if (!cekLevelLimbah()) break;
              mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
              pompaLimbahSet(true);
            } else {
              mulaiBilasSiklus();
            }
          }
          break;
        case L_DRAIN: {
          if (!cekLevelLimbah()) {
            pompaLimbahSet(false);
            break;
          }
          if (langkahLewat()) {
            pompaLimbahSet(false);
            tankTambahLimbah(volumeWadah + DRAIN_RESIDU_ML);
            kosongkanVolumeWadah();
            simpanTank();
            mulaiLangkah(L_DECIDE);
            mulaiBilasSiklus();
          }
          break;
        }
        case L_KONFIRMASI:
          // Menunggu pilihan pengguna (prosesKonfirmasiSiklus).
          break;
        default:
          break;
      }
      break;

    case FASE_BILAS_SABUN:
      switch (prosesLangkah) {
        case L_ISI:
          if (dcAktif(DC_SABUN) && !cekLevelSumber(3)) break;
          if (langkahLewat()) {
            pompaSabunSet(false);
            agitasiSet(p.agitasiSpeed);
            unsigned long durasi = (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
            mulaiLangkah(L_BILAS_AGITASI, durasi, durasi + 10000UL);
          }
          break;
        case L_BILAS_AGITASI:
          if (langkahLewat()) {
            agitasiSet(0);
            unsigned long ms = drainMsVolume(volumeWadah);
            if (ms == 0) { safetyRaiseKonteks(9, "Durasi drain 0 ms", "Cek model pompa limbah"); break; }
            if (!cekLevelLimbah()) break;
            mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
            pompaLimbahSet(true);
          }
          break;
        case L_DRAIN:
          if (!cekLevelLimbah()) { pompaLimbahSet(false); break; }
          if (langkahLewat()) {
            pompaLimbahSet(false);
            tankTambahLimbah(volumeWadah + DRAIN_RESIDU_ML);
            kosongkanVolumeWadah();
            simpanTank();
            mulaiBilasBerikut();
          }
          break;
        default: break;
      }
      break;

    case FASE_BILAS_AIR:
      switch (prosesLangkah) {
        case L_ISI:
          if (pompaAktif(2) && !cekLevelSumber(2)) break;
          if (pompaSelesai(2)) {
            agitasiSet(p.agitasiSpeed);
            unsigned long durasi = (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
            mulaiLangkah(L_BILAS_AGITASI, durasi, durasi + 10000UL);
          }
          break;
        case L_BILAS_AGITASI:
          if (langkahLewat()) {
            agitasiSet(0);
            unsigned long ms = drainMsVolume(volumeWadah);
            if (ms == 0) { safetyRaiseKonteks(9, "Durasi drain 0 ms", "Cek model pompa limbah"); break; }
            if (!cekLevelLimbah()) break;
            mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
            pompaLimbahSet(true);
          }
          break;
        case L_DRAIN:
          if (!cekLevelLimbah()) { pompaLimbahSet(false); break; }
          if (langkahLewat()) {
            pompaLimbahSet(false);
            tankTambahLimbah(volumeWadah + DRAIN_RESIDU_ML);
            kosongkanVolumeWadah();
            simpanTank();
            mulaiBilasBerikut();
          }
          break;
        default: break;
      }
      break;

    case FASE_DRAIN_BATAL:
      if (!cekLevelLimbah()) {
        pompaLimbahSet(false);
        break;
      }
      if (langkahLewat()) {
        pompaLimbahSet(false);
        tankTambahLimbah(volumeWadah + DRAIN_RESIDU_ML);
        kosongkanVolumeWadah();
        simpanTank();
        prosesJalan = false;
        prosesFase = FASE_IDLE;
        prosesLangkah = LANGKAH_NONE;
        prosesAkumMs = 0;
        prosesTotalMsEst = 0;
        hapusRunstate();
        buzzerNotif();
        perluGambar = true;
        dbg.println(F("[PROSES] Drain batal selesai"));
      }
      break;

    case FASE_PEMULIHAN:
      switch (prosesLangkah) {
        case L_DRAIN:
          if (!cekLevelLimbah()) { pompaLimbahSet(false); break; }
          if (langkahLewat()) {
            pompaLimbahSet(false);
            tankTambahLimbah(volumeWadah + DRAIN_RESIDU_ML);
            kosongkanVolumeWadah();
            simpanTank();
            if (pemulihanTahap == 1) {
              pemulihanTahap = 2;
              unsigned long est = pompaPerkiraanMs(2, p.bilasMl);
              mulaiLangkah(L_ISI, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
              if (!pompaMulai(2, p.bilasMl)) {
                safetyRaiseKonteks(9, "Pompa AIR gagal start", "Cek selang/prime");
              } else {
                tambahVolumeWadah(p.bilasMl);
                tankKurangi(2, p.bilasMl);
              }
            } else {
              pemulihanAktif = false;
              pemulihanTahap = 0;
              prosesFase = FASE_SELESAI;
              prosesLangkah = LANGKAH_NONE;
              hapusRunstate();
              buzzerSelesai();
              perluGambar = true;
            }
          }
          break;
        case L_ISI:
          if (pompaAktif(2) && !cekLevelSumber(2)) break;
          if (pompaSelesai(2)) {
            agitasiSet(p.agitasiSpeed);
            unsigned long durasi = (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
            mulaiLangkah(L_BILAS_AGITASI, durasi, durasi + 10000UL);
          }
          break;
        case L_BILAS_AGITASI:
          if (langkahLewat()) {
            agitasiSet(0);
            unsigned long ms = drainMsVolume(volumeWadah);
            if (ms == 0) { safetyRaiseKonteks(9, "Durasi drain 0 ms", "Cek model pompa limbah"); break; }
            if (!cekLevelLimbah()) break;
            pemulihanTahap = 3;
            mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
            pompaLimbahSet(true);
          }
          break;
        default: break;
      }
      break;

    case FASE_SELESAI:
      break;

    default:
      break;
  }
}
