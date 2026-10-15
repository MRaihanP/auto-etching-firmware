#pragma once
#include <Arduino.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "dbg.h"

#ifndef LED_PI
#define LED_PI 3.14159265358979323846
#endif

// ================= LED STRIP WS2812 (GPIO 7) =================
// Status indikator sistem: boot, proses, jeda, selesai, error, E-stop, peringatan.
// Non-blocking: animasi dijalankan di ledStripUpdate() tiap loop().

#define LED_STRIP_NUM_PIXELS  1   // jumlah LED (bisa dinaikkan utk strip panjang)
#define LED_STRIP_BRIGHTNESS  80  // 0-255 (batas utama; breathing modulate dari 0)

Adafruit_NeoPixel ledStrip(LED_STRIP_NUM_PIXELS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

// Warna GRB (Adafruit_NeoPixel::Color sudah mengonversi)
const uint32_t C_OFF     = 0;
const uint32_t C_RED     = ledStrip.Color(255, 0, 0);
const uint32_t C_GREEN   = ledStrip.Color(0, 255, 0);
const uint32_t C_BLUE    = ledStrip.Color(0, 0, 255);
const uint32_t C_YELLOW  = ledStrip.Color(255, 200, 0);

enum LedMode {
  LED_OFF,
  LED_BOOT,       // biru kelap-kelip: sedang boot
  LED_IDLE,       // hijau redup: siap, tidak ada proses
  LED_PROSES,     // biru breathing: proses berjalan
  LED_JEDA,       // kuning breathing: proses dijeda
  LED_SELESAI,    // hijau solid 2 detik lalu kembali IDLE
  LED_ERROR,      // merah kelap-kelip cepat: proses gagal
  LED_ESTOP,      // merah solid: E-stop aktif
  LED_WARNING     // kuning solid: peringatan (gas/suhu)
};

LedMode         ledMode      = LED_OFF;
unsigned long   ledMulaiMs   = 0;    // waktu masuk mode (untuk blink)
unsigned long   ledNextMs    = 0;    // jadwal update berikutnya
unsigned long   ledSelesaiMs = 0;    // kapan LED_SELESAI habis
bool            ledNyala     = false;// state blink (on/off)

void ledStripInit() {
  ledStrip.begin();
  ledStrip.setBrightness(0);
  ledStrip.clear();
  ledStrip.show();
  ledMode = LED_OFF;
}

void ledSetMode(LedMode m) {
  if (ledMode == m) return;
  ledMode    = m;
  ledMulaiMs = millis();
  ledNextMs  = ledMulaiMs;
  ledNyala   = false;

  switch (m) {
    case LED_IDLE:
    case LED_PROSES:
      ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
      break;
    case LED_SELESAI:
      ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
      ledStrip.fill(C_GREEN, 0, LED_STRIP_NUM_PIXELS);
      ledStrip.show();
      ledSelesaiMs = ledMulaiMs + 2000;   // 2 detik
      return;                              // tidak perlu update loop
    case LED_ESTOP:
    case LED_WARNING:
      ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
      break;
    default:
      ledStrip.setBrightness(0);
      break;
  }
  ledStrip.clear();
  ledStrip.show();
}

// Paksa restart: bypass guard "sudah di mode ini". Dipakai untuk LED_SELESAI
// supaya bisa dipanggil berulang dalam satu siklus proses.
void ledSetModePaksa(LedMode m) {
  ledMode = LED_OFF;
  ledSetMode(m);
}

// Breathing: level kecerahan 0..LED_STRIP_BRIGHTNESS, sinus halus.
uint8_t ledBreathing(unsigned long mulaiMs, unsigned long durasiMs) {
  if (durasiMs == 0) return LED_STRIP_BRIGHTNESS;
  float t = (float)((unsigned long)(millis() - mulaiMs) % durasiMs) / (float)durasiMs;
  return (uint8_t)((LED_STRIP_BRIGHTNESS * 0.5) +
                   (LED_STRIP_BRIGHTNESS * 0.5) * sinf(t * 2.0 * LED_PI));
}

void ledStripUpdate() {
  unsigned long now = millis();
  if ((long)(now - ledNextMs) < 0) return;

  switch (ledMode) {

    case LED_OFF:
      ledNextMs = now + 100;
      break;

    case LED_BOOT:
      // kelap-kelip biru 300 ms
      ledNyala = !ledNyala;
      ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
      if (ledNyala) ledStrip.fill(C_BLUE, 0, LED_STRIP_NUM_PIXELS);
      else          ledStrip.clear();
      ledStrip.show();
      ledNextMs = now + 300;
      break;

    case LED_IDLE:
      // hijau solid redup (setelah boot / selesai)
      if (!ledNyala) {
        ledNyala = true;
        ledStrip.setBrightness(LED_STRIP_BRIGHTNESS / 2);
        ledStrip.fill(C_GREEN, 0, LED_STRIP_NUM_PIXELS);
        ledStrip.show();
      }
      ledNextMs = now + 200;
      break;

    case LED_PROSES:
      // biru breathing halus (siklus ~1.6 s)
      ledStrip.setBrightness(ledBreathing(ledMulaiMs, 1600));
      ledStrip.fill(C_BLUE, 0, LED_STRIP_NUM_PIXELS);
      ledStrip.show();
      ledNextMs = now + 40;
      break;

    case LED_JEDA:
      // kuning breathing lambat (siklus ~2 s)
      ledStrip.setBrightness(ledBreathing(ledMulaiMs, 2000));
      ledStrip.fill(C_YELLOW, 0, LED_STRIP_NUM_PIXELS);
      ledStrip.show();
      ledNextMs = now + 40;
      break;

    case LED_SELESAI:
      // hijau solid 2 detik, lalu balik IDLE
      if ((long)(now - ledSelesaiMs) >= 0) ledSetMode(LED_IDLE);
      ledNextMs = now + 100;
      break;

    case LED_ERROR:
      // merah kelap-kelip cepat 120 ms
      ledNyala = !ledNyala;
      ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
      if (ledNyala) ledStrip.fill(C_RED, 0, LED_STRIP_NUM_PIXELS);
      else          ledStrip.clear();
      ledStrip.show();
      ledNextMs = now + 120;
      break;

    case LED_ESTOP:
      // merah solid
      if (!ledNyala) {
        ledNyala = true;
        ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
        ledStrip.fill(C_RED, 0, LED_STRIP_NUM_PIXELS);
        ledStrip.show();
      }
      ledNextMs = now + 100;
      break;

    case LED_WARNING:
      // kuning solid
      if (!ledNyala) {
        ledNyala = true;
        ledStrip.setBrightness(LED_STRIP_BRIGHTNESS);
        ledStrip.fill(C_YELLOW, 0, LED_STRIP_NUM_PIXELS);
        ledStrip.show();
      }
      ledNextMs = now + 100;
      break;
  }
}

// ---------- API singkat ----------
inline void ledBoot()    { ledSetMode(LED_BOOT); }
inline void ledIdle()    { ledSetMode(LED_IDLE); }
inline void ledProses()  { ledSetMode(LED_PROSES); }
inline void ledJeda()    { ledSetMode(LED_JEDA); }
inline void ledSelesai() { ledSetModePaksa(LED_SELESAI); }
inline void ledError()   { ledSetMode(LED_ERROR); }
inline void ledEstop()   { ledSetMode(LED_ESTOP); }
inline void ledWarning() { ledSetMode(LED_WARNING); }
inline void ledMatikan() { ledSetMode(LED_OFF); }