#pragma once
#include <Wire.h>
#include "config.h"

// Diisi di inputInit() mengikuti define P_xxx, jadi aman kalau pin ditukar
const char* namaTombol[JUMLAH_TOMBOL];

// Auto-repeat KIRI/KANAN: tahan tombol untuk mengubah nilai lebih cepat.
// Berguna saat memasukkan hasil ukur kalibrasi (langkah 0,1 ml). Tombol
// ATAS/BAWAH/ENTER/BACK tidak diulang agar navigasi tetap satu langkah.
#define TOMBOL_ULANG_DELAY_MS   600
#define TOMBOL_ULANG_PERIOD_MS  120

bool stateStabil[JUMLAH_TOMBOL];
bool stateTerbaca[JUMLAH_TOMBOL];
unsigned long waktuBerubah[JUMLAH_TOMBOL];
unsigned long waktuTekan[JUMLAH_TOMBOL];
bool sudahUlang[JUMLAH_TOMBOL];
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

// Inisialisasi state tombol dari kondisi pin saat ini
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
    waktuTekan[i]   = millis();
    sudahUlang[i]   = false;
  }
}

// Baca tombol + debounce + blocking (tombol pertama mengunci).
// Return index tombol yang baru saja valid ditekan, atau -1.
int bacaTombolBaru() {
  uint8_t data;
  if (!bacaPCF8574(data)) return TIDAK_ADA;

  int hasil = TIDAK_ADA;
  unsigned long sekarang = millis();

  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    bool mentah = !bitRead(data, i);   // active LOW

    if (mentah != stateTerbaca[i]) {
      waktuBerubah[i] = sekarang;
      stateTerbaca[i] = mentah;
    }

    if ((sekarang - waktuBerubah[i]) > DEBOUNCE_MS && mentah != stateStabil[i]) {
      stateStabil[i] = mentah;
      if (stateStabil[i]) {
        waktuTekan[i] = sekarang;
        sudahUlang[i] = false;
        if (tombolAktif == TIDAK_ADA) { tombolAktif = i; hasil = i; }
      } else {
        if (tombolAktif == i) tombolAktif = TIDAK_ADA;
      }
    }

    // Auto-repeat hanya untuk KIRI/KANAN selama tombol itu masih aktif.
    if (stateStabil[i] && (i == P_KIRI || i == P_KANAN) && tombolAktif == i) {
      unsigned long jeda = sudahUlang[i] ? TOMBOL_ULANG_PERIOD_MS
                                         : TOMBOL_ULANG_DELAY_MS;
      if (sekarang - waktuTekan[i] >= jeda) {
        waktuTekan[i] = sekarang;
        sudahUlang[i] = true;
        if (hasil == TIDAK_ADA) hasil = i;
      }
    }
  }
  return hasil;
}
