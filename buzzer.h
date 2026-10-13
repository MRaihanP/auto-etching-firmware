#pragma once
#include <Arduino.h>
#include "config.h"

// ================= BUZZER (GPIO6) =================
// Non-blocking: melodi dijalankan di buzzerUpdate() yang dipanggil tiap loop(),
// sehingga tidak pernah delay() dan tidak mengganggu logika proses/stepper.
// Nada dibangkitkan lewat LEDC PWM (buzzer pasif) agar frekuensi akurat.
//
// Cara bedain tiap event (bukan sekadar bunyi dengan pitch berbeda):
//   boot   : naik sama besar, 4 nada, Equal  -> "hidup"
//   mulai  : naik cepat 3 nada lalu tahan     -> "jalan"
//   selesai: fanfare, memuncak lalu kembali   -> "berhasil"
//   batal  : turun lambat, nada rendah       -> "berhenti/paksa"
// Frekuensi 0 = jeda (rest), bukan diam total.

void buzzerInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  ledcAttach(PIN_BUZZER, 1000, 10);          // 10 bit -> duty 0..1023
}

// duty: 0..100 (0 = diam). Durasi ditangani scheduler.
void buzzerNada(uint16_t freq, uint8_t duty) {
  if (freq == 0 || duty == 0) { ledcWrite(PIN_BUZZER, 0); return; }
  ledcWriteTone(PIN_BUZZER, freq);
  ledcWrite(PIN_BUZZER, (uint32_t)(1023UL * duty / 100));
}

void buzzerSenyap() { ledcWrite(PIN_BUZZER, 0); }

// ================= NOTA PEMBANTU =================
// Satu oktav di atas C4
#define NADA_C4 262
#define NADA_D4 294
#define NADA_E4 330
#define NADA_F4 349
#define NADA_G4 392
#define NADA_A4 440
#define NADA_B4 494
#define NADA_C5 523
#define NADA_D5 587
#define NADA_E5 659
#define NADA_F5 698
#define NADA_G5 784
#define NADA_A5 880
#define NADA_C6 1047
#define NADA_REST 0

// ================= MELODI =================
// Klik tombol: sangat singkat + duty kecil, terasa ringan.
const uint16_t NADA_KLIK[]  = { NADA_A5 };
const uint16_t MS_KLIK[]    = { 25 };
const uint8_t  DUTY_KLIK[]  = { 18 };

// Boot: C5 - E5 - G5 - C6, naik rata (persamaan bunyi "power on")
const uint16_t NADA_BOOT[]  = { NADA_C5, NADA_E5, NADA_G5, NADA_C6 };
const uint16_t MS_BOOT[]    = { 80, 80, 80, 220 };
const uint8_t  DUTY_BOOT[]  = { 40, 45, 50, 55 };

// Mulai: A4 - C5 - E5 cepat lalu A5 ditahan panjang (naik + tahan = "gas")
const uint16_t NADA_MULAI[] = { NADA_A4, NADA_C5, NADA_E5, NADA_A5 };
const uint16_t MS_MULAI[]   = { 60, 60, 60, 320 };
const uint8_t  DUTY_MULAI[] = { 45, 50, 55, 60 };

// Selesai: fanfare C5 - E5 - G5, jeda, lalu memuncak C6 dan ditutup G5 -> C6
const uint16_t NADA_SELESAI[] = {
  NADA_C5, NADA_E5, NADA_G5, NADA_REST, NADA_C6, NADA_G5, NADA_C6
};
const uint16_t MS_SELESAI[]   = { 90, 90, 90, 60, 130, 90, 420 };
const uint8_t  DUTY_SELESAI[] = { 45, 50, 55,  0,  60, 55,  65 };

// Batal: turun lambat di oktaf rendah A3 - F3 - D3 (nada "berhenti")
#define NADA_A3 220
#define NADA_F3 175
#define NADA_D3 147
const uint16_t NADA_BATAL[] = { NADA_A3, NADA_REST, NADA_F3, NADA_REST, NADA_D3 };
const uint16_t MS_BATAL[]   = { 140, 70, 140, 70, 340 };
const uint8_t  DUTY_BATAL[] = { 45,  0,  40,  0,  40 };

// ================= SCHEDULER =================
const uint16_t* nadaHz   = nullptr;
const uint16_t* nadaMs   = nullptr;
const uint8_t*  nadaDuty = nullptr;
int            nadaJumlah = 0;
int            nadaIndex  = 0;
unsigned long  nadaSelesaiMs = 0;
bool           nadaAktif  = false;

void buzzerPutar(const uint16_t* hz, const uint16_t* ms, const uint8_t* duty,
                 int jumlah) {
  nadaAktif = (jumlah > 0);
  if (!nadaAktif) { buzzerSenyap(); return; }
  nadaHz = hz; nadaMs = ms; nadaDuty = duty;
  nadaJumlah = jumlah;
  nadaIndex = 0;
  buzzerNada(hz[0], duty[0]);
  nadaSelesaiMs = millis() + ms[0];
}

void buzzerKlik()    { buzzerPutar(NADA_KLIK,   MS_KLIK,   DUTY_KLIK,   1); }
void buzzerBoot()    { buzzerPutar(NADA_BOOT,   MS_BOOT,   DUTY_BOOT,   4); }
void buzzerMulai()   { buzzerPutar(NADA_MULAI,  MS_MULAI,  DUTY_MULAI,  4); }
void buzzerSelesai() { buzzerPutar(NADA_SELESAI, MS_SELESAI, DUTY_SELESAI, 7); }
void buzzerBatal()   { buzzerPutar(NADA_BATAL,   MS_BATAL,   DUTY_BATAL,   5); }

void buzzerStop() {
  nadaAktif = false;
  nadaHz = nullptr;
  buzzerSenyap();
}

// Dipanggil tiap loop(): lanjutkan atau selesaikan melodi tanpa blocking.
void buzzerUpdate() {
  if (!nadaAktif) return;
  unsigned long now = millis();
  if ((long)(now - nadaSelesaiMs) < 0) return;      // masih dalam nada ini

  nadaIndex++;
  if (nadaIndex >= nadaJumlah) { buzzerStop(); return; }

  buzzerNada(nadaHz[nadaIndex], nadaDuty[nadaIndex]);
  nadaSelesaiMs = now + nadaMs[nadaIndex];
}