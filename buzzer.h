#pragma once
#include <Arduino.h>
#include "config.h"

// ================= BUZZER PASIF (GPIO6) =================
// Non-blocking: melodi dijalankan di buzzerUpdate() tiap loop.
// Note = {frekuensi Hz, durasi ms}. Frekuensi 0 = rest.

struct Note {
  uint16_t freq;
  uint16_t ms;
};

void buzzerInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  ledcAttach(PIN_BUZZER, 1000, 10);  // 10-bit PWM
}

void buzzerNada(uint16_t freq) {
  if (freq == 0) { ledcWrite(PIN_BUZZER, 0); return; }
  ledcWriteTone(PIN_BUZZER, freq);
  ledcWrite(PIN_BUZZER, 700);  // duty ~68%
}

void buzzerOff() { ledcWrite(PIN_BUZZER, 0); }

// ================= MELODI =================

const Note N_BOOT[]   = {{880, 80}, {1175, 80}, {1760, 250}};
const Note N_START[]  = {{1319, 100}, {1568, 120}};
const Note N_CANCEL[] = {{1568, 120}, {1319, 120}, {1047, 250}};
const Note N_DONE[]   = {{1047, 110}, {1319, 110}, {1568, 110}, {2093, 400}};
const Note N_KEY[]    = {{2093, 20}};
const Note N_WARN[]   = {{1760, 90}, {0, 60}, {1760, 90}};
const Note N_ERROR[]  = {{523, 220}, {0, 60}, {392, 220}, {0, 60}, {262, 400}};

// ================= SCHEDULER =================
const Note* melodi     = nullptr;
int        melodiJumlah = 0;
int        melodiIndex  = 0;
unsigned long melodiNextMs = 0;

void buzzerPutar(const Note* notes, int jumlah) {
  if (jumlah <= 0) { buzzerOff(); melodi = nullptr; return; }
  melodi = notes; melodiJumlah = jumlah; melodiIndex = 0;
  buzzerNada(notes[0].freq);
  melodiNextMs = millis() + notes[0].ms;
}

void buzzerKlik()    { buzzerPutar(N_KEY,    1); }
void buzzerBoot()    { buzzerPutar(N_BOOT,   3); }
void buzzerMulai()   { buzzerPutar(N_START,  2); }
void buzzerBatal()   { buzzerPutar(N_CANCEL, 3); }
void buzzerSelesai() { buzzerPutar(N_DONE,   4); }
void buzzerWarn()    { buzzerPutar(N_WARN,   3); }
void buzzerError()   { buzzerPutar(N_ERROR,  5); }

void buzzerStop() {
  melodi = nullptr;
  melodiJumlah = 0;
  buzzerOff();
}

// Dipanggil tiap loop(): lanjutkan atau selesaikan melodi tanpa blocking.
void buzzerUpdate() {
  if (melodi == nullptr) return;
  unsigned long now = millis();
  if ((long)(now - melodiNextMs) < 0) return;

  melodiIndex++;
  if (melodiIndex >= melodiJumlah) { buzzerStop(); return; }

  buzzerNada(melodi[melodiIndex].freq);
  melodiNextMs = now + melodi[melodiIndex].ms;
}
