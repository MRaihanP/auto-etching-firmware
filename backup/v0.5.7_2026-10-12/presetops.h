#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

#define JUMLAH_FIELD 11
const char* namaField[JUMLAH_FIELD] = {
  "PCB panjang", "PCB lebar", "Mode dosing",
  "%HCl", "%H2O2", "Volume",
  "Mode agitasi", "Durasi agitasi", "Speed agit",
  "Durasi total", "Nama"
};

void hitungUlang(Preset &p, int vol) {
  if (p.modeDosing == 0) {
    p.persenHCl = 47; p.persenH2O2 = 24; p.persenH2O = 29;
  } else {
    p.persenH2O = 100 - p.persenHCl - p.persenH2O2;
  }
  p.mlHCl  = (int)((long)vol * p.persenHCl  / 100);
  p.mlH2O2 = (int)((long)vol * p.persenH2O2 / 100);
  p.mlH2O  = vol - p.mlHCl - p.mlH2O2;
}

int volumeTotal(const Preset &p) { return p.mlHCl + p.mlH2O2 + p.mlH2O; }

void ubahField(Preset &p, int f, int arah) {
  int vol = volumeTotal(p);
  switch (f) {
    case 0: p.pcbPanjang = constrain(p.pcbPanjang + arah * 5, 20, 300); break;
    case 1: p.pcbLebar   = constrain(p.pcbLebar   + arah * 5, 20, 300); break;
    case 2:
      p.modeDosing = (p.modeDosing + arah + 2) % 2;
      hitungUlang(p, vol);
      break;
    case 3:
      if (p.modeDosing == 1) {
        p.persenHCl = constrain(p.persenHCl + arah, 0, 100 - p.persenH2O2);
        hitungUlang(p, vol);
      }
      break;
    case 4:
      if (p.modeDosing == 1) {
        p.persenH2O2 = constrain(p.persenH2O2 + arah, 0, 100 - p.persenHCl);
        hitungUlang(p, vol);
      }
      break;
    case 5:
      vol = constrain(vol + arah * 10, VOL_MIN, VOL_MAX);
      hitungUlang(p, vol);
      break;
    case 6:
      p.modeAgitasi = (p.modeAgitasi + arah + 2) % 2;
      if (p.modeAgitasi == 0) {
        p.agitasiDetik = constrain(30 + vol / 10, 30, 180);
      }
      break;
    case 7:
      if (p.modeAgitasi == 1)
        p.agitasiDetik = constrain(p.agitasiDetik + arah, 1, 180);
      break;
    case 8: p.agitasiSpeed = constrain(p.agitasiSpeed + arah, 1, 5); break;
    case 9: p.totalDetik   = constrain(p.totalDetik + arah * 10, 30, 3600); break;
    case 10: break;
  }
}

bool validasiPreset(const Preset &p) {
  if (memchr(p.nama, 0, sizeof(p.nama)) == NULL) return false;
  if (p.pcbPanjang < 20 || p.pcbPanjang > 300) return false;
  if (p.pcbLebar   < 20 || p.pcbLebar   > 300) return false;
  if (p.persenHCl < 0 || p.persenH2O2 < 0 || p.persenH2O < 0) return false;
  if (p.persenHCl + p.persenH2O2 + p.persenH2O != 100) return false;
  int vol = volumeTotal(p);
  if (vol < VOL_MIN || vol > VOL_MAX) return false;
  if (p.agitasiDetik < 1 || p.agitasiDetik > 180) return false;
  if (p.agitasiSpeed < 1 || p.agitasiSpeed > 5) return false;
  if (p.totalDetik < 30 || p.totalDetik > 3600) return false;
  if (p.modeDosing > 1 || p.modeAgitasi > 1) return false;
  return true;
}

void resetKeDefault() {
  for (int i = 0; i < JUMLAH_DEFAULT; i++) daftarPreset[i] = presetDefault[i];
  jumlahPreset = JUMLAH_DEFAULT;
}

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
  p.modeDosing = 0; p.modeAgitasi = 0;
  hitungUlang(p, 170);
  p.agitasiDetik = 60; p.agitasiSpeed = 3;
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