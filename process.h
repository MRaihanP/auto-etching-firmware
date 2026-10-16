#pragma once
#include <Arduino.h>
#include "data.h"
#include "motor.h"
#include "dcmotor.h"
#include "buzzer.h"
#include "led_strip.h"

// ================= STATE MACHINE PROSES =================
// Logika proses berjalan di prosesUpdate() (dipanggil tiap loop),
// TERPISAH dari render. Tiap transisi fase juga mengubah mode LED strip
// (biru breathing = kerja, merah = E-stop/gagal, hijau = selesai).

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

// ================= SEMUA AKTUATOR MATI =================
// Satu-satunya jalan untuk mematikan semua output. Dipakai oleh aksiFase
// (SELESAI), prosesBatal(), dan handler E-stop di .ino — ketiga tempat
// sebelumnya punya 4 baris stop yang identik (duplikasi).
void semuaAktuatorMati() {
  agitasiSet(0);
  pompaLimbahSet(false);
  pompaSabunSet(false);
  fanSet(false);
  pompaMatikanTotal();          // pompaStopAll() + stepperSleep(true) -> EN = HIGH
}

// Aksi hardware saat memasuki sebuah fase
void aksiFase(FaseProses f) {
  Preset &p = daftarPreset[presetTerpilih];
  switch (f) {
    case FASE_DOSING:
      ledProses();                    // biru breathing: kerja
      dcmotorStopAll();
      pompaMulai(0, p.mlHCl);   // HCl
      pompaMulai(1, p.mlH2O2);  // H2O2
      pompaMulai(2, p.mlH2O);   // H2O
      break;
    case FASE_ETCHING:
      ledProses();
      pompaStopAll();
      agitasiSet(p.agitasiSpeed);
      break;
    case FASE_DRAINING:
      ledProses();
      agitasiSet(0);
      pompaLimbahSet(true);
      break;
    case FASE_RINSING:
      ledProses();
      pompaLimbahSet(false);
      agitasiSet(p.agitasiSpeed);
      pompaMulai(2, 100);       // bilas dengan air
      break;
    case FASE_SELESAI:
      semuaAktuatorMati();
      break;
    default: break;
  }
}

bool prosesJalan = false;          // true = proses aktif (termasuk saat jeda)
bool prosesJeda  = false;
bool dialogBatal = false;          // dialog konfirmasi batal sedang tampil
unsigned long prosesAkumMs      = 0;   // waktu proses, DIBATASI totalMs (progress & fase)
// Waktu proses sebenarnya tanpa batas. `prosesAkumMs` dijepit ke totalMs supaya
// fraksi progress selalu 0..1, tapi justru karena itu ia selalu identik dengan
// `totalDetik` di akhir proses — tidak berguna untuk laporan "durasi aktual".
// Variabel ini yang dipakai layar selesai.
unsigned long prosesWaktuMs     = 0;
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

// Batas tiap fase sebagai fraksi dari total preset. Tunggal dengan
// `faseDariFrac()` supaya progress bar per fase dan perpindahan fase tidak
// pernah bertentangan (mis. bar menunjukkan 100% sementara fase belum
// berganti). Index = nilai enum FaseProses.
const float FASE_AWAL_F[6]  = { 0.00, 0.10, 0.75, 0.88, 1.00, 1.00 };
const float FASE_AKHIR_F[6] = { 0.10, 0.75, 0.88, 1.00, 1.00, 1.00 };

// Progres keseluruhan proses, 0..1. Dijepit supaya tidak pernah > 1 walau
// waktu nyata melampaui total preset.
float prosesFraksiTotal() {
  int total = prosesTotalDetik();
  if (total <= 0) return 0;
  float f = (float)prosesAkumMs / ((float)total * 1000.0f);
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  return f;
}

// Progres 0..1 DI DALAM fase yang sedang berjalan — inilah yang dipakai
// loading bar, supaya operator melihat "tinggal berapa lama fase ini"
// alih-alih barra yang selalu terlihat di paruh atas selama fase panjang.
float prosesFraksiFase() {
  int k = prosesFase;
  if (k < 0 || k > FASE_SELESAI) return 0;
  float a = FASE_AWAL_F[k], z = FASE_AKHIR_F[k];
  if (z - a < 0.0001f) return 1.0f;          // fase SELESAI/IDLE = sudah penuh
  float v = (prosesFraksiTotal() - a) / (z - a);
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  return v;
}

void prosesMulai() {
  prosesJalan = true;
  prosesJeda = false;
  dialogBatal = false;
  prosesAkumMs = 0;
  prosesWaktuMs = 0;
  pompaResetTotal();              // total ml terpompa dihitung ulang tiap proses
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
  ledJeda();                      // kuning breathing saat dijeda
  if (!prosesJeda) aksiFase(prosesFase);   // lanjut: kembalikan LED ke biru breathing
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
  semuaAktuatorMati();           // agitasi + pompa + fan + stepper EN=LOW->HIGH
  ledError();                     // merah kelap-kelip: proses dibatalkan/gagal
  perluGambar = true;
}

// Dipanggil setelah proses SELESAI dan user keluar dari layar proses
void prosesKeluarSelesai() {
  prosesJalan = false;
  prosesFase = FASE_IDLE;
  ledIdle();                     // hijau redup: siap lagi
  perluGambar = true;
}

// Panggil tiap loop(), tidak tergantung render
void prosesUpdate() {
  if (!prosesJalan) return;

  unsigned long now = millis();
  if (!prosesJeda && !prosesSelesai()) {
    unsigned long d = now - prosesTerakhirMs;
    prosesWaktuMs += d;
    prosesAkumMs  += d;
  }
  prosesTerakhirMs = now;

  unsigned long totalMs = (unsigned long)prosesTotalDetik() * 1000UL;
  if (prosesAkumMs > totalMs) prosesAkumMs = totalMs;   // progress selalu 0..1

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
    if (f == FASE_SELESAI) {
      buzzerSelesai();              // nada selesai proses
      ledSelesai();                 // hijau solid 2 detik
      dialogBatal = false;          // selesai -> dialog batal tidak relevan
    }
    perluGambar = true;
  }
}
