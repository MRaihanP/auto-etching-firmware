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

void tankTambahLimbah(int ml) {
  if (ml <= 0) return;
  tankLimbahMl += ml;
  if (tankLimbahMl > TANGKI_MAKS_ML) tankLimbahMl = TANGKI_MAKS_ML;
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
  if (tankLimbahMl > TANGKI_MAKS_ML - TANK_PERINGATAN_ML) return true;
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
    ms += (unsigned long)SKALA_UJI_CEPAT(p.agitasiDetik) * 1000UL;
    ms += dcDrainMs(volumeTotal(p));
    // Bilas sabun + air PER SIKLUS
    for (int i = 0; i < p.bilasSabun; i++) {
      ms += dcDosisMs(0, p.bilasMl);
      ms += (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
      ms += dcDrainMs(p.bilasMl);
    }
    for (int i = 0; i < p.bilasAir; i++) {
      ms += pompaPerkiraanMs(2, p.bilasMl);
      ms += (unsigned long)SKALA_UJI_CEPAT(p.bilasDetik) * 1000UL;
      ms += dcDrainMs(p.bilasMl);
    }
  }
  return (int)(ms / 1000UL);
}

int prosesTotalDetik() {
  return estimasiTotalDetikPreset(daftarPreset[presetTerpilih]);
}

bool prosesSelesai() { return prosesFase == FASE_SELESAI; }
bool prosesDrainBatalAktif() { return prosesJalan && prosesFase == FASE_DRAIN_BATAL; }

// ---------- simpan status pemulihan ----------
void simpanRunstateSekarang() {
  simpanRunstate((int)prosesFase, (int)prosesLangkah,
                 prosesSiklus, volumeWadah, presetTerpilih);
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
    safetyRaise(2);
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
// Pemeriksaan sensor saat proses berjalan. Fault harus menang atas "kering"
// agar sensor rusak tidak disalahartikan sebagai tangki kosong.
bool cekLevelSumber(int i) {
  if (levelFault(i)) { safetyRaise(3); return false; }
  if (levelKering(i)) { safetyRaise(5); return false; }
  return true;
}

bool cekLevelLimbah() {
  if (levelFault(4)) { safetyRaise(3); return false; }
  if (levelBasah(4)) { safetyRaise(6); return false; }
  return true;
}

bool precheckLulus() {
  Preset &p = daftarPreset[presetTerpilih];

  // Stepper wajib terkalibrasi. Pompa limbah TIDAK wajib: drain punya
  // fallback waktu. Pompa sabun wajib bila preset memakai bilas sabun.
  if (!pompaSiapSemua()) {
    safetyRaise(9);
    return false;
  }
  if (p.bilasSabun > 0 && kalibDcMlPerSec[0] <= 0.0f) {
    safetyRaise(9);
    return false;
  }
  if (!sensorSuhuValid(2)) {
    safetyRaise(3);
    return false;
  }
  // Sumber: sensor level wajib ada dan berisi.
  for (int i = 0; i < 3; i++) {
    if (levelFault(i)) { safetyRaise(3); return false; }
    if (levelKering(i)) { safetyRaise(5); return false; }
  }
  if (p.bilasSabun > 0) {
    if (levelFault(3)) { safetyRaise(3); return false; }
    if (levelKering(3)) { safetyRaise(5); return false; }
  }
  // Limbah tidak boleh sudah penuh.
  if (levelFault(4)) { safetyRaise(3); return false; }
  if (levelBasah(4)) { safetyRaise(6); return false; }

  // Pelacak tangki (dipakai bila pengguna sudah mengisi volume awal).
  long butuhHcl   = (long)p.siklus * p.mlHCl;
  long butuhH2O2  = (long)p.siklus * p.mlH2O2;
  long butuhH2O   = (long)p.siklus *
                    ((long)p.mlH2O + (long)p.bilasAir * p.bilasMl);
  long butuhSabun = (long)p.siklus * p.bilasSabun * p.bilasMl;
  if (tankHclMl > 0 && tankHclMl < butuhHcl) { safetyRaise(5); return false; }
  if (tankH2o2Ml > 0 && tankH2o2Ml < butuhH2O2) { safetyRaise(5); return false; }
  if (tankH2oMl > 0 && tankH2oMl < butuhH2O) { safetyRaise(5); return false; }
  if (tankSabunMl > 0 && tankSabunMl < butuhSabun) { safetyRaise(5); return false; }

  long totalCair = (long)p.siklus *
                   (volumeTotal(p) +
                    (long)(p.bilasSabun + p.bilasAir) * p.bilasMl);
  if (tankLimbahMl + totalCair > TANGKI_MAKS_ML) {
    safetyRaise(6);
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
  perluGambar = true;
  dbg.println(F("[PROSES] Selesai"));
}

// Mulai isi bilas sabun (pompa DC). Volume dicatat saat mulai agar cancel
// di tengah pengisian tetap tahu wadah sudah berisi.
void mulaiIsiSabun() {
  Preset &p = daftarPreset[presetTerpilih];
  unsigned long d = dcDosisMs(0, p.bilasMl);
  if (d == 0) { safetyRaise(9); return; }
  mulaiLangkah(L_ISI, d, d + PROSES_TIMEOUT_MARGIN_MS);
  pompaSabunSet(true);
  volumeWadah += p.bilasMl;
  tankKurangi(3, p.bilasMl);
}

void mulaiIsiAir() {
  Preset &p = daftarPreset[presetTerpilih];
  unsigned long est = pompaPerkiraanMs(2, p.bilasMl);
  mulaiLangkah(L_ISI, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
  if (!pompaMulai(2, p.bilasMl)) {
    safetyRaise(9);
  } else {
    volumeWadah += p.bilasMl;
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
    safetyRaise(9);
  } else {
    // Dicatat saat mulai, bukan saat selesai: bila proses dibatalkan di
    // tengah dosing, volume wadah tetap diperhitungkan untuk drain.
    volumeWadah += p.mlH2O;
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
    dbg.println(F("[PROSES] Jeda (semua aktuator)"));
  } else {
    unsigned long delta = millis() - jedaMulaiMs;
    langkahMulaiMs += delta;
    if (langkahBatasMs) langkahBatasMs += delta;
    pompaLanjutSemua();
    dcLanjutSemua();
    dbg.println(F("[PROSES] Lanjut"));
  }
  perluGambar = true;
}

// Mulai drain wadah yang sedang berisi (dipakai cancel dan sisa proses).
void mulaiDrainWadah() {
  unsigned long ms = dcDrainMs(volumeWadah);
  prosesJalan = true;
  prosesJeda = false;
  prosesFase = FASE_DRAIN_BATAL;
  prosesLangkah = L_DRAIN;
  prosesAkumMs = 0;
  prosesTotalMsEst = ms;
  prosesTerakhirMs = millis();
  mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
  pompaLimbahSet(true);
  dbg.print(F("[PROSES] Drain wadah "));
  dbg.print(volumeWadah);
  dbg.println(F(" ml"));
}

// E-stop/ERROR: semua motor mati, lalu pompa limbah menyedot cairan wadah.
// Durasi memakai model limbah atau fallback waktu bila belum dikalibrasi.
void mulaiEvakuasiDarurat() {
  if (evakuasiAktif) return;
  if (volumeWadah <= 0) return;
  // Jangan menyalakan pompa bila sensor menyatakan tangki limbah penuh.
  if (safetyKode == 6 || levelBasah(4)) {
    dbg.println(F("[EVAKUASI] Dibatalkan: tangki limbah penuh"));
    return;
  }
  unsigned long ms = dcDrainMs(volumeWadah);
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

  if (levelBasah(4)) {
    dcStop(DC_LIMBAH);
    evakuasiAktif = false;
    dbg.println(F("[EVAKUASI] Stop: tangki limbah penuh"));
    perluGambar = true;
    return;
  }
  if (millis() - evakuasiMulaiMs < evakuasiDurasiMs) {
    float duty = (kalibDutyDC[1] > 0) ? (float)kalibDutyDC[1] : 100.0f;
    if (dcTargetPct[DC_LIMBAH] < 1.0f) dcSetPct(DC_LIMBAH, duty);
    return;
  }

  dcStop(DC_LIMBAH);
  tankTambahLimbah(evakuasiMl);
  volumeWadah  = 0;
  evakuasiMl   = 0;
  evakuasiAktif = false;
  simpanTank();
  hapusRunstate();
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
    // Jangan hapus runstate di sini: bila wadah masih berisi, runstate sudah
    // disimpan di atas agar boot berikutnya menawarkan drain.
    simpanTank();
    // Bila error terjadi saat pemulihan, tawarkan pemulihan lagi setelah
    // pengguna membersihkan error, memakai volume wadah saat ini.
    if (pemulihanAktif) {
      pemulihanAktif = false;
      pemulihanVolume = volumeWadah;
      pemulihanTersedia = true;
    }
    dbg.println(F("[PROSES] Dibatalkan darurat"));
  }
  // E-stop/ERROR: nyalakan pompa limbah sampai cairan tersedot.
  if (safetyState != SAF_AMAN) mulaiEvakuasiDarurat();
  perluGambar = true;
}

// Batal oleh pengguna: matikan semua motor lain, lalu LANGSUNG drain wadah.
void prosesBatal() {
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

  if (adaIsi && safetyBolehAktuator()) {
    mulaiDrainWadah();
  } else {
    if (adaIsi) simpanRunstateSekarang(); else hapusRunstate();
    prosesJalan = false;
    prosesFase = FASE_IDLE;
    prosesLangkah = LANGKAH_NONE;
    simpanTank();
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
}

void prosesAbaikanPemulihan() {
  // Abaikan = tidak menjalankan drain otomatis, tetapi volume wadah tetap
  // dilacak agar proses berikutnya tidak dimulai di atas sisa cairan.
  volumeWadah = constrain(pemulihanVolume, 0, TANGKI_MAKS_ML);
  pemulihanTersedia = false;
  pemulihanAktif = false;
  hapusRunstate();
  perluGambar = true;
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
  prosesFase = FASE_PEMULIHAN;
  prosesSiklus = pemulihanSiklus > 0 ? pemulihanSiklus : 1;
  prosesAkumMs = 0;
  prosesTerakhirMs = millis();
  volumeWadah = constrain(pemulihanVolume, 0, TANGKI_MAKS_ML);
  prosesTotalMsEst = 0;

  unsigned long ms = dcDrainMs(volumeWadah);
  if (volumeWadah > 0) {
    if (ms == 0) {
      // Ada isi wadah tetapi durasi drain tak bisa dihitung. Jangan lewati
      // drain: hentikan dan minta pengguna melengkapi kalibrasi pompa DC.
      dbg.println(F("[PROSES] Pemulihan ditolak: kalibrasi DC belum lengkap"));
      pemulihanAktif = false;
      pemulihanTersedia = true;   // tawarkan pemulihan lagi setelah error clear
      safetyRaise(9);
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
      safetyRaise(9);
    } else {
      volumeWadah += p.bilasMl;
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
    dbg.print(F("[PROSES] Timeout "));
    dbg.println(namaLangkah[prosesLangkah]);
    safetyRaise(4);
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
              safetyRaise(9);
            } else {
              volumeWadah += p.mlH2O2;
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
              safetyRaise(9);
            } else {
              volumeWadah += p.mlHCl;
              tankKurangi(0, p.mlHCl);
            }
          }
          break;
        case L_DOSE_HCL:
          if (pompaAktif(0) && !cekLevelSumber(0)) break;
          if (pompaSelesai(0)) {
            agitasiSet(p.agitasiSpeed);
            unsigned long durasi = (unsigned long)SKALA_UJI_CEPAT(p.agitasiDetik) * 1000UL;
            mulaiLangkah(L_AGITASI, durasi, durasi + 10000UL);
          }
          break;
        case L_AGITASI:
          if (langkahLewat()) {
            agitasiSet(0);
            if (volumeWadah > 0) {
              unsigned long ms = dcDrainMs(volumeWadah);
              if (ms == 0) { safetyRaise(9); break; }
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
            tankTambahLimbah(volumeWadah);
            volumeWadah = 0;
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
            unsigned long ms = dcDrainMs(volumeWadah);
            if (ms == 0) { safetyRaise(9); break; }
            if (!cekLevelLimbah()) break;
            mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
            pompaLimbahSet(true);
          }
          break;
        case L_DRAIN:
          if (!cekLevelLimbah()) { pompaLimbahSet(false); break; }
          if (langkahLewat()) {
            pompaLimbahSet(false);
            tankTambahLimbah(volumeWadah);
            volumeWadah = 0;
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
            unsigned long ms = dcDrainMs(volumeWadah);
            if (ms == 0) { safetyRaise(9); break; }
            if (!cekLevelLimbah()) break;
            mulaiLangkah(L_DRAIN, ms, ms + PROSES_TIMEOUT_MARGIN_MS);
            pompaLimbahSet(true);
          }
          break;
        case L_DRAIN:
          if (!cekLevelLimbah()) { pompaLimbahSet(false); break; }
          if (langkahLewat()) {
            pompaLimbahSet(false);
            tankTambahLimbah(volumeWadah);
            volumeWadah = 0;
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
        tankTambahLimbah(volumeWadah);
        volumeWadah = 0;
        simpanTank();
        prosesJalan = false;
        prosesFase = FASE_IDLE;
        prosesLangkah = LANGKAH_NONE;
        prosesAkumMs = 0;
        prosesTotalMsEst = 0;
        hapusRunstate();
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
            tankTambahLimbah(volumeWadah);
            volumeWadah = 0;
            simpanTank();
            if (pemulihanTahap == 1) {
              pemulihanTahap = 2;
              unsigned long est = pompaPerkiraanMs(2, p.bilasMl);
              mulaiLangkah(L_ISI, 0, est ? est * 2UL + PROSES_TIMEOUT_MARGIN_MS : 0);
              if (!pompaMulai(2, p.bilasMl)) {
                safetyRaise(9);
              } else {
                volumeWadah += p.bilasMl;
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
            unsigned long ms = dcDrainMs(volumeWadah);
            if (ms == 0) { safetyRaise(9); break; }
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
