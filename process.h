#pragma once
#include <Arduino.h>
#include "data.h"
#include "motor.h"
#include "dcmotor.h"
#include "buzzer.h"

// ================= STATE MACHINE PROSES (masih simulasi) =================
// Logika proses berjalan di prosesUpdate() (dipanggil tiap loop),
// TERPISAH dari render. Aktuator belum ada: fase hanya label + log.

enum FaseProses {
  FASE_IDLE,
  FASE_DOSING,
  FASE_ETCHING,
  FASE_DRAINING,
  FASE_RINSING,
  FASE_SELESAI
};
const char* namaFase[] = {
  "IDLE", "DOSING", "ETCHING", "DRAINING", "RINSING", "SELESAI"
};

// Aksi hardware saat memasuki sebuah fase
void aksiFase(FaseProses f) {
  Preset &p = daftarPreset[presetTerpilih];
  switch (f) {
    case FASE_DOSING:
      dcmotorStopAll();
      pompaMulai(0, p.mlHCl);   // HCl
      pompaMulai(1, p.mlH2O2);  // H2O2
      pompaMulai(2, p.mlH2O);   // H2O
      break;
    case FASE_ETCHING:
      pompaStopAll();
      agitasiSet(p.agitasiSpeed);
      break;
    case FASE_DRAINING:
      agitasiSet(0);
      pompaLimbahSet(true);
      break;
    case FASE_RINSING:
      pompaLimbahSet(false);
      agitasiSet(p.agitasiSpeed);
      pompaMulai(2, 100);       // bilas dengan air
      break;
    case FASE_SELESAI:
      agitasiSet(0);
      pompaLimbahSet(false);
      pompaSabunSet(false);
      fanSet(false);
      pompaStopAll();
      break;
    default: break;
  }
}

bool prosesJalan = false;          // true = proses aktif (termasuk saat jeda)
bool prosesJeda  = false;
bool dialogBatal = false;          // dialog konfirmasi batal sedang tampil
unsigned long prosesAkumMs      = 0;   // waktu proses berjalan (tanpa waktu jeda)
unsigned long prosesTerakhirMs  = 0;
int  prosesDetikTerakhir        = -1;
FaseProses prosesFase           = FASE_IDLE;

int prosesTotalDetik() { return daftarPreset[presetTerpilih].totalDetik; }

bool prosesSelesai() { return prosesFase == FASE_SELESAI; }

FaseProses faseDariFrac(float f) {
  if (f < 0.10) return FASE_DOSING;
  if (f < 0.75) return FASE_ETCHING;
  if (f < 0.88) return FASE_DRAINING;
  return FASE_RINSING;
}

void prosesMulai() {
  prosesJalan = true;
  prosesJeda = false;
  dialogBatal = false;
  prosesAkumMs = 0;
  prosesTerakhirMs = millis();
  prosesDetikTerakhir = -1;
  prosesFase = FASE_DOSING;
  dbg.print(F("[PROSES] Mulai: "));
  dbg.print(daftarPreset[presetTerpilih].nama);
  dbg.print(F(" ("));
  dbg.print(prosesTotalDetik());
  dbg.println(F(" s)"));
  dbg.println(F("[PROSES] Fase: DOSING"));
  aksiFase(FASE_DOSING);
  buzzerMulai();                 // nada konfirmasi mulai
  perluGambar = true;
}

void prosesToggleJeda() {
  if (!prosesJalan || prosesSelesai()) return;
  prosesJeda = !prosesJeda;
  dbg.println(prosesJeda ? F("[PROSES] Jeda") : F("[PROSES] Lanjut"));
  perluGambar = true;
}

void prosesBatal() {
  dbg.print(F("[PROSES] Dibatalkan pada detik ke-"));
  dbg.println(prosesAkumMs / 1000);
  buzzerBatal();                 // nada konfirmasi batal
  prosesJalan = false;
  prosesJeda = false;
  dialogBatal = false;
  prosesFase = FASE_IDLE;
  agitasiSet(0);
  pompaLimbahSet(false);
  pompaSabunSet(false);
  fanSet(false);
  pompaStopAll();
  perluGambar = true;
}

// Dipanggil setelah proses SELESAI dan user keluar dari layar proses
void prosesKeluarSelesai() {
  prosesJalan = false;
  prosesFase = FASE_IDLE;
  perluGambar = true;
}

// Panggil tiap loop(), tidak tergantung render
void prosesUpdate() {
  if (!prosesJalan) return;

  unsigned long now = millis();
  if (!prosesJeda && !prosesSelesai()) prosesAkumMs += now - prosesTerakhirMs;
  prosesTerakhirMs = now;

  unsigned long totalMs = (unsigned long)prosesTotalDetik() * 1000UL;
  if (prosesAkumMs > totalMs) prosesAkumMs = totalMs;

  int d = prosesAkumMs / 1000;
  if (d != prosesDetikTerakhir) {
    prosesDetikTerakhir = d;
    perluGambar = true;
  }

  FaseProses f = (prosesAkumMs >= totalMs)
                   ? FASE_SELESAI
                   : faseDariFrac((float)prosesAkumMs / (float)totalMs);
  if (f != prosesFase) {
    prosesFase = f;
    dbg.print(F("[PROSES] Fase: "));
    dbg.println(namaFase[f]);
    aksiFase(f);
    if (f == FASE_SELESAI) buzzerSelesai();   // nada selesai proses
    if (f == FASE_SELESAI) dialogBatal = false;   // selesai -> dialog batal tidak relevan
    perluGambar = true;
  }
}
