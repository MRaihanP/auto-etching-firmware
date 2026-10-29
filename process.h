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
      dcStopAll();
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
      {
        int mlBuang = p.mlHCl + p.mlH2O2 + p.mlH2O;
        unsigned long ms = dcDosisMs(1, mlBuang);
        if (ms > 0) {
          dbg.print(F("[PROSES] Perkiraan buang "));
          dbg.print(mlBuang);
          dbg.print(F(" ml = "));
          dbg.print(ms / 1000);
          dbg.println(F(" s (termasuk margin)"));
        }
      }
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
unsigned long dosingMulaiMs     = 0;   // awal fase DOSING (untuk timeout)
unsigned long dosingBatasMs     = 0;   // batas waktu dosing (0 = tidak dihitung)
unsigned long dosingJedaMulaiMs = 0;   // awal jeda terakhir (timeout dikoreksi saat lanjut)
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

  // Timeout dosing: 2 x estimasi terlama + margin.
  Preset &p = daftarPreset[presetTerpilih];
  unsigned long estimasi = 0;
  estimasi = max(estimasi, pompaPerkiraanMs(0, p.mlHCl));
  estimasi = max(estimasi, pompaPerkiraanMs(1, p.mlH2O2));
  estimasi = max(estimasi, pompaPerkiraanMs(2, p.mlH2O));
  dosingMulaiMs = millis();
  dosingBatasMs = (estimasi > 0) ? (estimasi * 2UL + PUMP_TIMEOUT_MARGIN_MS) : 0;

  dbg.print(F("[PROSES] Mulai: "));
  dbg.print(p.nama);
  dbg.print(F(" ("));
  dbg.print(prosesTotalDetik());
  dbg.println(F(" s)"));
  dbg.print(F("[PROSES] Estimasi dosing: "));
  dbg.print(estimasi);
  dbg.println(F(" ms"));
  dbg.println(F("[PROSES] Fase: DOSING"));
  aksiFase(FASE_DOSING);
  buzzerMulai();                 // nada konfirmasi mulai
  perluGambar = true;
}

void prosesToggleJeda() {
  if (!prosesJalan || prosesSelesai()) return;
  prosesJeda = !prosesJeda;
  if (prosesJeda) {
    pompaJedaSemua();
    dosingJedaMulaiMs = millis();
    dbg.println(F("[PROSES] Jeda"));
  } else {
    // Waktu jeda tidak dihitung terhadap timeout dosing.
    dosingMulaiMs += millis() - dosingJedaMulaiMs;
    pompaLanjutSemua();
    dbg.println(F("[PROSES] Lanjut"));
  }
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

  // Fase DOSING: tunggu pompa benar-benar selesai, jangan hanya ikut fraksi waktu.
  // Timeout mencegah pompa berjalan tanpa henti bila ada masalah.
  if (prosesFase == FASE_DOSING && !prosesJeda) {
    if (dosingBatasMs > 0 && (now - dosingMulaiMs) > dosingBatasMs) {
      dbg.println(F("[PROSES] Timeout dosing -> batal"));
      pompaStopAllDarurat();        // kondisi fault: potong seketika
      prosesBatal();
      return;
    }
    unsigned long batasFase = totalMs * 10UL / 100UL;
    if (!semuaPompaSelesai()) {
      // Tahan di akhir fase DOSING sampai ketiga pompa selesai.
      if (batasFase > 0 && prosesAkumMs >= batasFase) prosesAkumMs = batasFase - 1;
    } else {
      // Semua pompa selesai lebih awal -> lanjut ke ETCHING.
      if (prosesAkumMs < batasFase) prosesAkumMs = batasFase;
    }
  }

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
