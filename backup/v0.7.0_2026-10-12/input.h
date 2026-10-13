#pragma once
#include <Wire.h>
#include "config.h"

const char* namaTombol[JUMLAH_TOMBOL];
bool stateStabil[JUMLAH_TOMBOL];
bool stateTerbaca[JUMLAH_TOMBOL];
unsigned long waktuBerubah[JUMLAH_TOMBOL];
int tombolAktif = TIDAK_ADA;

bool bacaPCF8574(uint8_t &data) {
  Wire.requestFrom(PCF8574_ADDR, 1);
  if (Wire.available()) { data = Wire.read(); return true; }
  return false;
}

bool tulisPCF8574(uint8_t data) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}

void inputInit() {
  namaTombol[P_ATAS]  = "ATAS";
  namaTombol[P_BAWAH] = "BAWAH";
  namaTombol[P_KIRI]  = "KIRI";
  namaTombol[P_KANAN] = "KANAN";
  namaTombol[P_ENTER] = "ENTER";
  namaTombol[P_BACK]  = "BACK";
  uint8_t data = 0xFF;
  bacaPCF8574(data);
  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    bool tertekan = !bitRead(data, i);
    stateStabil[i]  = tertekan;
    stateTerbaca[i] = tertekan;
    waktuBerubah[i] = millis();
  }
}

int bacaTombolBaru() {
  uint8_t data;
  if (!bacaPCF8574(data)) return TIDAK_ADA;
  int hasil = TIDAK_ADA;
  unsigned long sekarang = millis();
  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    bool mentah = !bitRead(data, i);
    if (mentah != stateTerbaca[i]) {
      waktuBerubah[i] = sekarang;
      stateTerbaca[i] = mentah;
    }
    if ((sekarang - waktuBerubah[i]) > DEBOUNCE_MS && mentah != stateStabil[i]) {
      stateStabil[i] = mentah;
      if (stateStabil[i]) {
        if (tombolAktif == TIDAK_ADA) { tombolAktif = i; hasil = i; }
      } else {
        if (tombolAktif == i) tombolAktif = TIDAK_ADA;
      }
    }
  }
  return hasil;
}