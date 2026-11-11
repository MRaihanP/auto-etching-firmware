#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"
#include "motor.h"
#include "dcmotor.h"
#include "buzzer.h"

// ================= MODUL KESELAMATAN =================
// State global + latch E-stop. Semua tindakan yang menyalakan aktuator wajib
// memeriksa safetyBolehAktuator().
//
// E-stop dibaca lewat interrupt (hanya menandai flag) lalu di-debounce 20 ms.
// Setelah aktif, state tetap ESTOP sampai tombol DILEPAS dan pengguna menekan
// ENTER pada layar E-STOP.

enum SafetyState { SAF_AMAN = 0, SAF_ESTOP = 1, SAF_ERROR = 2 };

SafetyState safetyState = SAF_AMAN;
int         safetyKode  = 0;
const char* safetyPesan = "";
// Detail spesifik agar pengguna tahu penyebab dan tindakannya.
char        safetyDetail[48] = "";
char        safetyAksi[32]   = "";

void safetySalin(char* dst, size_t n, const char* src) {
  if (dst == nullptr || n == 0) return;
  if (src == nullptr) { dst[0] = '\0'; return; }
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

volatile bool safetyIsrFlag = false;
unsigned long safetyIsrMs = 0;
unsigned long safetyDebounceMs = 0;
bool          safetyTombolDitekan = false;

// Didefinisikan di process.h; dipanggil sekali saat transisi darurat.
void prosesBatalDarurat();
void sesuaikanVolumeAktual();
// Evakuasi cairan lewat pompa limbah saat E-stop/ERROR (process.h).
extern bool evakuasiAktif;

struct SafetyFaultInfo {
  int kode;
  const char* nama;
  const char* pesan;
};

const SafetyFaultInfo daftarFault[] = {
  { 0,  "OK",           "Tidak ada kesalahan" },
  { 1,  "E01 ESTOP",    "Tombol darurat ditekan" },
  { 2,  "E02 TEMP_HIGH","Suhu melebihi batas bahaya" },
  { 3,  "E03 SENSOR",   "Sensor wajib gagal" },
  { 4,  "E04 TIMEOUT",  "Langkah proses melebihi batas waktu" },
  { 5,  "E05 TANK",     "Tangki kosong atau sisa kurang" },
  { 6,  "E06 WASTE",    "Tangki limbah penuh" },
  { 7,  "E07 NVS",      "Data penyimpanan rusak" },
  { 8,  "E08 RESUME",   "Proses terputus, butuh keputusan" },
  { 9,  "E09 CAL",      "Kalibrasi pompa belum lengkap" },
  { 10, "E10 WDT",      "Watchdog memicu reset" }
};
const int JUMLAH_FAULT = sizeof(daftarFault) / sizeof(daftarFault[0]);

const SafetyFaultInfo& safetyInfo(int kode) {
  for (int i = 0; i < JUMLAH_FAULT; i++) if (daftarFault[i].kode == kode) return daftarFault[i];
  return daftarFault[0];
}

// IRAM_ATTR: rutin interrupt harus tetap aman walau flash sibuk.
void IRAM_ATTR safetyEstopISR() {
  safetyIsrFlag = true;
  safetyIsrMs = millis();
}

void safetyInit() {
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), safetyEstopISR, FALLING);
  safetyState = SAF_AMAN;
  safetyKode = 0;
  safetyPesan = "";
  safetyIsrFlag = false;
  safetyDebounceMs = millis();
  dbg.println(F("[SAFETY] E-stop interrupt aktif"));
}

void safetyMatikanAktuator() {
  // Catat volume dosis/drain nyata sebelum pompa dihentikan.
  sesuaikanVolumeAktual();
  pompaStopAllDarurat();
  dcStopAllDarurat();
  kalibDcFase = KDC_IDLE;
  kalibDcIdx  = -1;
  kalibDcUkur = false;
  kalibStepperUkur = false;
  prosesBatalDarurat();
}

// Dipanggil sekali saat transisi ke ESTOP/ERROR.
void safetyDarurat(int kode, const char* pesan) {
  if (safetyState != SAF_AMAN) return;   // hanya satu kali per kejadian
  safetyState = SAF_ESTOP;
  safetyKode = kode;
  safetyPesan = pesan;
  safetySalin(safetyDetail, sizeof(safetyDetail), "Tombol darurat ditekan");
  safetySalin(safetyAksi, sizeof(safetyAksi), "Lepas tombol, lalu ENTER");
  safetyTombolDitekan = true;
  safetyMatikanAktuator();
  buzzerError();
  perluGambar = true;
  dbg.print(F("[SAFETY] E-STOP: "));
  dbg.println(pesan);
  eventCatat(1, "ESTOP");
}

void safetyRaiseKonteks(int kode, const char* detail, const char* aksi) {
  if (safetyState != SAF_AMAN) return;
  const SafetyFaultInfo& f = safetyInfo(kode);
  safetyState = SAF_ERROR;
  safetyKode = kode;
  safetyPesan = f.pesan;
  safetySalin(safetyDetail, sizeof(safetyDetail), detail);
  safetySalin(safetyAksi, sizeof(safetyAksi), aksi);
  safetyMatikanAktuator();
  buzzerError();
  perluGambar = true;
  dbg.print(F("[SAFETY] ERROR "));
  dbg.print(f.nama);
  dbg.print(F(": "));
  if (detail != nullptr && detail[0]) {
    dbg.print(detail);
    dbg.print(F(" | "));
  }
  dbg.print(safetyPesan);
  if (aksi != nullptr && aksi[0]) {
    dbg.print(F(" | aksi: "));
    dbg.print(aksi);
  }
  dbg.println();
  eventCatat((uint16_t)kode, (detail != nullptr && detail[0]) ? detail : f.nama);
}

void safetyRaise(int kode) { safetyRaiseKonteks(kode, "", ""); }

bool safetyBolehAktuator() { return safetyState == SAF_AMAN; }

// Clear dari dashboard hanya untuk E-stop, setelah tombol fisik dilepas
// dan evakuasi cairan selesai. Error non-E-stop tetap harus lokal.
bool safetyClearRemoteBoleh() {
  return safetyState == SAF_ESTOP &&
         digitalRead(PIN_ESTOP) == HIGH &&
         !evakuasiAktif;
}

// ENTER di layar E-STOP/ERROR. E-stop hanya bisa dilepas bila tombol fisik
// sudah tidak ditekan.
bool safetyKonfirmasi() {
  if (safetyState == SAF_AMAN) return false;
  // Jangan izinkan reset selama pompa limbah masih mengevakuasi cairan.
  if (evakuasiAktif) {
    dbg.println(F("[SAFETY] Evakuasi cairan masih berjalan"));
    return false;
  }
  if (safetyState == SAF_ESTOP && digitalRead(PIN_ESTOP) == LOW) {
    dbg.println(F("[SAFETY] E-stop masih ditekan"));
    return false;
  }
  safetyState = SAF_AMAN;
  safetyKode = 0;
  safetyPesan = "";
  safetyDetail[0] = '\0';
  safetyAksi[0] = '\0';
  safetyIsrFlag = false;
  perluGambar = true;
  dbg.println(F("[SAFETY] State kembali AMAN"));
  return true;
}

// Debounce interrupt: kejadian baru sah bila pin masih LOW setelah 20 ms.
void safetyTick() {
  bool pinLow = (digitalRead(PIN_ESTOP) == LOW);

  if (safetyIsrFlag) {
    safetyIsrFlag = false;
    if (pinLow) safetyDebounceMs = millis();
  }

  if (safetyState != SAF_ESTOP && pinLow &&
      millis() - safetyDebounceMs >= 20UL) {
    safetyDarurat(1, safetyInfo(1).pesan);
  }
  if (!pinLow) safetyTombolDitekan = false;
}
