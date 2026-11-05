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

volatile bool safetyIsrFlag = false;
unsigned long safetyIsrMs = 0;
unsigned long safetyDebounceMs = 0;
bool          safetyTombolDitekan = false;

// Didefinisikan di process.h; dipanggil sekali saat transisi darurat.
void prosesBatalDarurat();

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
  safetyTombolDitekan = true;
  safetyMatikanAktuator();
  buzzerError();
  perluGambar = true;
  dbg.print(F("[SAFETY] E-STOP: "));
  dbg.println(pesan);
}

void safetyRaise(int kode) {
  if (safetyState != SAF_AMAN) return;
  const SafetyFaultInfo& f = safetyInfo(kode);
  safetyState = SAF_ERROR;
  safetyKode = kode;
  safetyPesan = f.pesan;
  safetyMatikanAktuator();
  buzzerError();
  perluGambar = true;
  dbg.print(F("[SAFETY] ERROR "));
  dbg.print(f.nama);
  dbg.print(F(": "));
  dbg.println(f.pesan);
}

bool safetyBolehAktuator() { return safetyState == SAF_AMAN; }

// ENTER di layar E-STOP/ERROR. E-stop hanya bisa dilepas bila tombol fisik
// sudah tidak ditekan.
bool safetyKonfirmasi() {
  if (safetyState == SAF_AMAN) return false;
  if (safetyState == SAF_ESTOP && digitalRead(PIN_ESTOP) == LOW) {
    dbg.println(F("[SAFETY] E-stop masih ditekan"));
    return false;
  }
  safetyState = SAF_AMAN;
  safetyKode = 0;
  safetyPesan = "";
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
