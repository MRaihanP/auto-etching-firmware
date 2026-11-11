#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= FIELD EDIT (Preset v3) =================
enum FieldPreset {
  FP_PCB_PANJANG = 0,
  FP_PCB_LEBAR,
  FP_MODE_RASIO,      // 0 = persen, 1 = ml
  FP_HCL,
  FP_H2O2,
  FP_H2O,             // otomatis
  FP_VOLUME,          // volume total campuran
  FP_ISI_TEMBAGA,     // % luas PCB yang tertutup tembaga
  FP_TEBAL_TEMBAGA,   // um
  FP_LAJU_ETCH,       // 0,1 um/menit
  FP_MODE_WAKTU,      // 0 = manual, 1 = otomatis
  FP_ETCHING,         // detik per siklus (manual)
  FP_SPEED,
  FP_SIKLUS,
  FP_BILAS_SABUN,
  FP_BILAS_AIR,
  FP_BILAS_ML,
  FP_BILAS_DETIK,
  FP_KONFIRMASI,
  FP_JUMLAH
};
#define JUMLAH_FIELD FP_JUMLAH
const char* namaField[JUMLAH_FIELD] = {
  "PCB panjang", "PCB lebar", "Mode rasio", "HCl", "H2O2", "H2O",
  "Volume", "Isi tembaga", "Tebal Cu", "Laju etch", "Mode waktu",
  "Etching", "Speed agit", "Siklus",
  "Bilas sabun", "Bilas air", "Bilas ml", "Bilas detik", "Konfirmasi"
};

// Hitung ulang %H2O dan ml dari persen HCl/H2O2 + volume total.
// ml H2O = sisa, jadi total ml selalu = vol.
void hitungUlang(Preset &p, int vol) {
  p.persenH2O = 100 - p.persenHCl - p.persenH2O2;
  p.mlHCl  = (int)((long)vol * p.persenHCl  / 100);
  p.mlH2O2 = (int)((long)vol * p.persenH2O2 / 100);
  p.mlH2O  = vol - p.mlHCl - p.mlH2O2;
}

int volumeTotal(const Preset &p) { return p.mlHCl + p.mlH2O2 + p.mlH2O; }

int persenDariMl(int ml, int vol) {
  if (vol <= 0) return 0;
  return (int)((long)ml * 100 / vol);   // pembulatan ke bawah: jumlah <= 100
}

// Mode ml: persen hanya untuk tampilan, diturunkan dari ml.
void sinkronPersen(Preset &p) {
  int vol = volumeTotal(p);
  p.persenHCl  = persenDariMl(p.mlHCl, vol);
  p.persenH2O2 = persenDariMl(p.mlH2O2, vol);
  p.persenH2O  = 100 - p.persenHCl - p.persenH2O2;
}

// ---------- geometri & model waktu etching ----------
long luasPcbCm2(const Preset &p) {
  return (long)p.pcbPanjang * p.pcbLebar / 100;   // mm2 -> cm2
}

long luasTembagaCm2(const Preset &p) {
  return luasPcbCm2(p) * p.isiTembagaPct / 100;
}

// Tebal tembaga disimpan dalam um, tetapi UI memakai oz (0,1 oz).
// 1 oz = 35 um, jadi 17,5 um dibulatkan ke 18 um saat diedit.
int tebalOzX10(const Preset &p) {
  return (p.tebalTembagaUm * 10 + 17) / 35;
}

int ozX10KeUm(int ozX10) {
  return (ozX10 * 35 + 5) / 10;
}

// Kebutuhan kimia minimum (ml HCl 33% dan H2O2 17%) untuk melarutkan
// tembaga: 0,094 ml per cm2 pada foil 35 um. Nilai ini hanya informasi
// (bukan dasar dosing) karena volume nyata ditentukan geometri wadah.
float kimiaMinMl(const Preset &p) {
  float perCm2 = 0.094f * (float)p.tebalTembagaUm / 35.0f;
  return (float)luasTembagaCm2(p) * perCm2;
}

float tembagaGram(const Preset &p) {
  return (float)luasTembagaCm2(p) * (float)p.tebalTembagaUm / 10000.0f * 8.96f;
}

// Waktu etching per siklus. Mode otomatis:
//   detik = (tebal / laju) x 60 x faktor aman / jumlah siklus
int agitasiDetikEfektif(const Preset &p) {
  if (p.modeWaktu == 0) return p.agitasiDetik;
  float laju = (float)p.lajuEtchX10 / 10.0f;   // um/menit
  if (laju <= 0.0f) return p.agitasiDetik;
  int siklus = (p.siklus > 0) ? p.siklus : 1;
  float detik = ((float)p.tebalTembagaUm / laju) * 60.0f *
                ((float)ETCH_FAKTOR_AMAN_PCT / 100.0f) / (float)siklus;
  return constrain((int)(detik + 0.5f), 30, 1800);
}

// arah = -1 (KIRI) / +1 (KANAN). Nilai selalu di-clamp ke rentang valid.
void ubahField(Preset &p, int f, int arah) {
  int vol = volumeTotal(p);
  switch (f) {
    case FP_PCB_PANJANG: p.pcbPanjang = constrain(p.pcbPanjang + arah * 5, 20, 300); break;
    case FP_PCB_LEBAR:   p.pcbLebar   = constrain(p.pcbLebar   + arah * 5, 20, 300); break;
    case FP_MODE_RASIO:
      p.modeRasio = p.modeRasio ? 0 : 1;
      if (p.modeRasio == 0) sinkronPersen(p);
      break;
    case FP_HCL:
      if (p.modeRasio == 0) {
        p.persenHCl = constrain(p.persenHCl + arah, 0, 100 - p.persenH2O2);
        hitungUlang(p, vol);
      } else {
        p.mlHCl = constrain(p.mlHCl + arah * 5, 0, max(0, vol - p.mlH2O2));
        p.mlH2O = vol - p.mlHCl - p.mlH2O2;
        sinkronPersen(p);
      }
      break;
    case FP_H2O2:
      if (p.modeRasio == 0) {
        p.persenH2O2 = constrain(p.persenH2O2 + arah, 0, 100 - p.persenHCl);
        hitungUlang(p, vol);
      } else {
        p.mlH2O2 = constrain(p.mlH2O2 + arah * 5, 0, max(0, vol - p.mlHCl));
        p.mlH2O = vol - p.mlHCl - p.mlH2O2;
        sinkronPersen(p);
      }
      break;
    case FP_H2O: break;   // otomatis: sisa volume
    case FP_VOLUME:
      if (p.modeRasio == 0) {
        vol = constrain(vol + arah * 10, VOL_MIN, VOL_MAX);
        hitungUlang(p, vol);
      } else {
        int minVol = max((int)VOL_MIN, p.mlHCl + p.mlH2O2);
        vol = constrain(vol + arah * 10, minVol, VOL_MAX);
        p.mlH2O = vol - p.mlHCl - p.mlH2O2;
        sinkronPersen(p);
      }
      break;
    case FP_ISI_TEMBAGA:
      p.isiTembagaPct = constrain(p.isiTembagaPct + arah * 5, 5, 90); break;
    case FP_TEBAL_TEMBAGA: {
      // Langkah 0,5 oz; batas 0,5..6,0 oz (18..210 um).
      int oz = constrain(tebalOzX10(p) + arah * 5, 5, 60);
      p.tebalTembagaUm = ozX10KeUm(oz);
      break;
    }
    case FP_LAJU_ETCH:
      p.lajuEtchX10 = constrain(p.lajuEtchX10 + arah * 5, 5, 600); break;
    case FP_MODE_WAKTU:
      p.modeWaktu = p.modeWaktu ? 0 : 1; break;
    case FP_ETCHING:
      p.agitasiDetik = constrain(p.agitasiDetik + arah * 10, 30, 1800); break;
    case FP_SPEED: p.agitasiSpeed = constrain(p.agitasiSpeed + arah, 1, 3); break;
    case FP_SIKLUS: p.siklus = constrain(p.siklus + arah, 1, 6); break;
    case FP_BILAS_SABUN: p.bilasSabun = constrain(p.bilasSabun + arah, 0, 3); break;
    case FP_BILAS_AIR: p.bilasAir = constrain(p.bilasAir + arah, 0, 5); break;
    case FP_BILAS_ML: p.bilasMl = constrain(p.bilasMl + arah * 10, 20, VOL_MAX); break;
    case FP_BILAS_DETIK: p.bilasDetik = constrain(p.bilasDetik + arah * 5, 5, 300); break;
    case FP_KONFIRMASI: p.konfirmasiSiklus = (p.konfirmasiSiklus ? 0 : 1); break;
  }
}

// ================= VALIDASI =================
bool validasiPreset(const Preset &p) {
  if (memchr(p.nama, 0, sizeof(p.nama)) == NULL) return false;   // harus null-terminated
  if (p.pcbPanjang < 20 || p.pcbPanjang > 300) return false;
  if (p.pcbLebar   < 20 || p.pcbLebar   > 300) return false;
  if (p.persenHCl < 0 || p.persenH2O2 < 0 || p.persenH2O < 0) return false;
  if (p.persenHCl + p.persenH2O2 + p.persenH2O != 100) return false;
  if (p.mlHCl < 0 || p.mlH2O2 < 0 || p.mlH2O < 0) return false;
  int vol = volumeTotal(p);
  if (vol < VOL_MIN || vol > VOL_MAX) return false;
  if (p.modeRasio < 0 || p.modeRasio > 1) return false;
  if (p.isiTembagaPct < 5 || p.isiTembagaPct > 90) return false;
  if (p.tebalTembagaUm < 10 || p.tebalTembagaUm > 210) return false;
  if (p.lajuEtchX10 < 5 || p.lajuEtchX10 > 600) return false;
  if (p.modeWaktu < 0 || p.modeWaktu > 1) return false;
  if (p.agitasiDetik < 30 || p.agitasiDetik > 1800) return false;
  if (p.agitasiSpeed < 1 || p.agitasiSpeed > 3) return false;
  if (p.siklus < 1 || p.siklus > 6) return false;
  if (p.bilasSabun < 0 || p.bilasSabun > 3) return false;
  if (p.bilasAir < 0 || p.bilasAir > 5) return false;
  if (p.bilasMl < 20 || p.bilasMl > VOL_MAX) return false;
  if (p.bilasDetik < 5 || p.bilasDetik > 300) return false;
  if (p.konfirmasiSiklus < 0 || p.konfirmasiSiklus > 1) return false;
  return true;
}

// ================= OPERASI DAFTAR PRESET =================
void resetKeDefault() {
  for (int i = 0; i < JUMLAH_DEFAULT; i++) daftarPreset[i] = presetDefault[i];
  jumlahPreset = JUMLAH_DEFAULT;
}

// Nama unik "Preset N" (N terkecil yang belum dipakai)
void namaBaru(char* out) {
  for (int n = 1; n < 100; n++) {
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "Preset %d", n);
    bool dipakai = false;
    for (int i = 0; i < jumlahPreset; i++)
      if (strcmp(daftarPreset[i].nama, tmp) == 0) { dipakai = true; break; }
    if (!dipakai) { strcpy(out, tmp); return; }
  }
  strcpy(out, "Preset");
}

bool tambahPreset() {
  if (jumlahPreset >= MAKS_PRESET) return false;
  Preset p;
  memset(&p, 0, sizeof(p));
  namaBaru(p.nama);
  p.pcbPanjang = 100; p.pcbLebar = 80;
  p.persenHCl = 33;   p.persenH2O2 = 17;
  hitungUlang(p, 360);
  p.agitasiDetik = 300; p.agitasiSpeed = 3;
  p.siklus = 3;
  p.bilasSabun = 1; p.bilasAir = 2;
  p.bilasMl = 100;  p.bilasDetik = 20;
  p.konfirmasiSiklus = 0;
  p.modeRasio = 0;
  p.isiTembagaPct = 40;
  p.tebalTembagaUm = 35;
  p.lajuEtchX10 = ETCH_LAJU_DEFAULT_X10;
  p.modeWaktu = 1;
  daftarPreset[jumlahPreset++] = p;
  return true;
}

bool duplikatPreset(int idx) {
  if (jumlahPreset >= MAKS_PRESET || idx < 0 || idx >= jumlahPreset) return false;
  Preset p = daftarPreset[idx];
  namaBaru(p.nama);
  daftarPreset[jumlahPreset++] = p;
  return true;
}

bool hapusPreset(int idx) {
  if (idx < 0 || idx >= jumlahPreset) return false;
  for (int i = idx; i < jumlahPreset - 1; i++) daftarPreset[i] = daftarPreset[i + 1];
  jumlahPreset--;
  return true;
}
