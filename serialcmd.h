#pragma once
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "dbg.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "process.h"
#include "motor.h"
#include "dcmotor.h"

// ================= KONTROL VIA SERIAL (v0.7.0) =================
// Mengontrol alat dari Serial Monitor tanpa pencet tombol — berguna untuk pengujian
// otomatis dan debug jarak jauh.
//
// Format: awali '>', lalu teks perintah, diakhiri Enter (CR/LF).
//   >help        daftar perintah
//   >status      fase proses, waktu, level 5 tangki, status sensor
//   >start       mulai proses dengan preset terpilih
//   >stop        batalkan proses
//   >preset N    pilih preset N (1..jumlahPreset)
//   >sel <hal>   pindah halaman: menu | preset | cfg | level | kalib
//   >off         matikan semua aktuator + stepper (setara E-stop software)
//
// NON-BLOCKING: parser membaca SATU karakter per iterasi loop() dan langsung
// memproses begitu baris lengkap. Sengaja TIDAK memakai `Serial.readString()` —
// fungsi itu menunggu Enter sambil memblokir, sehingga seluruh sistem membeku.
//
// Prefix '>' dipakai supaya log biasa dari `dbg.h` tidak ikut diparse.

#define CMD_MAX_LEN 32

char    cmdBuf[CMD_MAX_LEN];
uint8_t cmdLen = 0;

// `namaSensor[]` dideklarasikan di sensor.h; dipakai untuk printsStatus.
extern const char* namaSensor[JUMLAH_SENSOR];

bool cmdCocok(const char* s) {
  return strcasecmp(cmdBuf, s) == 0;
}

// ---------- Pretty print ----------

void cmdPrintHelp() {
  dbg.println(F("=== PERINTAH (awali '>', Enter) ==="));
  dbg.println(F("  >help        daftar perintah ini"));
  dbg.println(F("  >status      fase, waktu, level 5 tangki, status sensor"));
  dbg.println(F("  >start       mulai proses dengan preset terpilih"));
  dbg.println(F("  >stop        batalkan proses"));
  dbg.println(F("  >preset N    pilih preset N (1..jumlahPreset)"));
  dbg.println(F("  >sel <hal>   pindah: menu|preset|cfg|level|kalib"));
  dbg.println(F("  >off         matikan semua aktuator + stepper"));
}

void cmdPrintStatus() {
  dbg.println(F("=== STATUS ==="));
  dbg.print(F("halaman  : ")); dbg.println(namaHalaman[halaman]);
  dbg.print(F("preset   : ")); dbg.println(daftarPreset[presetTerpilih].nama);

  if (prosesJalan) {
    dbg.print(F("proses   : "));
    dbg.println(namaFase[prosesFase]);
    if (prosesJeda) dbg.println(F("           (JEDA)"));
    dbg.print(F("  waktu  : ")); dbg.print(prosesAkumMs / 1000); dbg.print(F(" dtk"));
    dbg.print(F(" / "));         dbg.println(prosesTotalDetik());
    dbg.print(F("  pompa  : "));
    dbg.print(pompaJalan[0] ? "HCl " : "-- ");
    dbg.print(pompaJalan[1] ? "H2O2 " : "-- ");
    dbg.println(pompaJalan[2] ? "H2O" : "--");
  } else {
    dbg.println(F("proses   : tidak berjalan"));
  }

  dbg.println(F("--- level tangki ---"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbg.print(F("  "));
    dbg.print(namaSensor[i]);
    while (strlen(namaSensor[i]) + 1 < 8) dbg.print(' ');
    dbg.print(sensorPersen[i]);
    dbg.print(F("% "));
    if (sensorMati[i])           dbg.println(F("[MATI]"));
    else if (sensorAdaCairan(i))  dbg.println(F("[ISI]"));
    else                         dbg.println(F("[KOSONG]"));
  }

  dbg.print(F("jumlah   : "));
  dbg.print(jumlahPreset);
  dbg.println(F(" buah preset"));
}

// ---------- Eksekusi perintah ----------

void cmdSelHalaman(const char* hal) {
  if      (strcasecmp(hal, "menu")   == 0) pindahHalaman(HAL_MENU_UTAMA);
  else if (strcasecmp(hal, "preset") == 0) pindahHalaman(HAL_PRESET_LIST);
  else if (strcasecmp(hal, "cfg")    == 0) pindahHalaman(HAL_CFG_LIST);
  else if (strcasecmp(hal, "level")  == 0) { levelMasuk(); pindahHalaman(HAL_LEVEL); }
  else if (strcasecmp(hal, "kalib")  == 0) pindahHalaman(HAL_KALIB_MAIN);
  else dbg.println(F("halaman tidak dikenal: menu|preset|cfg|level|kalib"));
}

void cmdJalankan(char* baris) {
  // buang spasi di depan & belakang
  while (*baris == ' ') baris++;
  char* akhir = baris + strlen(baris);
  while (akhir > baris && akhir[-1] == ' ') *--akhir = '\0';
  if (*baris == '\0') return;

  // salin ke cmdBuf supaya bisa dibandingkan case-insensitive
  strncpy(cmdBuf, baris, CMD_MAX_LEN - 1);
  cmdBuf[CMD_MAX_LEN - 1] = '\0';

  // pisahkan argumen pertama (kata kedua)
  char* arg = strchr(cmdBuf, ' ');
  if (arg) *arg++ = '\0';

  if (cmdCocok("help")) { cmdPrintHelp(); return; }
  if (cmdCocok("status")) { cmdPrintStatus(); return; }

  if (cmdCocok("start")) {
    if (prosesJalan) { dbg.println(F("proses sudah berjalan")); return; }
    prosesMulai();
    pindahHalaman(HAL_PROSES);
    dbg.print(F("[CMD] Mulai: "));
    dbg.println(daftarPreset[presetTerpilih].nama);
    return;
  }

  if (cmdCocok("stop")) {
    if (!prosesJalan) { dbg.println(F("proses tidak berjalan")); return; }
    prosesBatal();
    dbg.println(F("[CMD] Proses dibatalkan"));
    return;
  }

  if (cmdCocok("preset")) {
    if (!arg) { dbg.println(F("syntax: >preset N")); return; }
    int n = atoi(arg);
    if (n < 1 || n > jumlahPreset) {
      dbg.print(F("[CMD] Preset tidak ada (1.."));
      dbg.print(jumlahPreset);
      dbg.println(F(")"));
      return;
    }
    presetTerpilih = n - 1;
    dbg.print(F("[CMD] Preset: "));
    dbg.println(daftarPreset[presetTerpilih].nama);
    return;
  }

  if (cmdCocok("sel")) {
    if (!arg) { dbg.println(F("syntax: >sel menu|preset|cfg|level|kalib")); return; }
    cmdSelHalaman(arg);
    return;
  }

  if (cmdCocok("off")) {
    semuaAktuatorMati();
    dbg.println(F("[CMD] Semua aktuator dimatikan"));
    return;
  }

  dbg.print(F("[CMD] Perintah tidak dikenal: "));
  dbg.println(cmdBuf);
  dbg.println(F("      ketik >help"));
}

// ---------- Parser non-blocking ----------

// Panggil tiap iterasi loop(). Regenerasi baris selesai (Enter) langsung
// dieksekusi; karakter lain hanya ditumpuk.
void serialCmdUpkeep() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '>') {
      cmdLen = 0;                 // awal baris perintah baru
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        cmdJalankan(cmdBuf);
        cmdLen = 0;
      }
      continue;
    }
    if (cmdLen < CMD_MAX_LEN - 1) cmdBuf[cmdLen++] = c;
    // kalau buffer penuh, karakter berikutnya dibuang sampai Enter
  }
}