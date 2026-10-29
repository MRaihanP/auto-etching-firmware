#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= FIELD EDIT =================
#define JUMLAH_FIELD 9
const char* namaField[JUMLAH_FIELD] = {
  "PCB panjang", "PCB lebar", "HCl", "H2O2", "H2O",
  "Volume", "Agitasi", "Speed agit", "Durasi"
};
// idx: 0 panjang, 1 lebar, 2 %HCl, 3 %H2O2, 4 %H2O (otomatis, tidak bisa diubah),
//      5 volume total, 6 agitasi (s), 7 speed, 8 durasi total (s)

// Hitung ulang %H2O dan ml dari persen HCl/H2O2 + volume total.
// ml H2O = sisa, jadi total ml selalu = vol.
void hitungUlang(Preset &p, int vol) {
  p.persenH2O = 100 - p.persenHCl - p.persenH2O2;
  p.mlHCl  = (int)((long)vol * p.persenHCl  / 100);
  p.mlH2O2 = (int)((long)vol * p.persenH2O2 / 100);
  p.mlH2O  = vol - p.mlHCl - p.mlH2O2;
}

int volumeTotal(const Preset &p) { return p.mlHCl + p.mlH2O2 + p.mlH2O; }

// arah = -1 (KIRI) / +1 (KANAN). Nilai selalu di-clamp ke rentang valid.
void ubahField(Preset &p, int f, int arah) {
  int vol = volumeTotal(p);
  switch (f) {
    case 0: p.pcbPanjang = constrain(p.pcbPanjang + arah * 5, 20, 300); break;
    case 1: p.pcbLebar   = constrain(p.pcbLebar   + arah * 5, 20, 300); break;
    case 2:
      p.persenHCl = constrain(p.persenHCl + arah, 0, 100 - p.persenH2O2);
      hitungUlang(p, vol); break;
    case 3:
      p.persenH2O2 = constrain(p.persenH2O2 + arah, 0, 100 - p.persenHCl);
      hitungUlang(p, vol); break;
    case 4: break;   // otomatis
    case 5:
      vol = constrain(vol + arah * 10, VOL_MIN, VOL_MAX);
      hitungUlang(p, vol); break;
    case 6: p.agitasiDetik = constrain(p.agitasiDetik + arah, 1, 60); break;
    case 7: p.agitasiSpeed = constrain(p.agitasiSpeed + arah, 1, 3); break;
    case 8: p.totalDetik   = constrain(p.totalDetik + arah * 10, 30, 3600); break;
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
  if (p.agitasiDetik < 1 || p.agitasiDetik > 60) return false;
  if (p.agitasiSpeed < 1 || p.agitasiSpeed > 5) return false;
  if (p.totalDetik < 30 || p.totalDetik > 3600) return false;
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
  p.agitasiDetik = 5; p.agitasiSpeed = 3;
  p.totalDetik = 270;
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
