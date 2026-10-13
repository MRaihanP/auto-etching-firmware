#pragma once
#include <Arduino.h>
#include "data.h"
#include "sensor.h"
#include "aktuator.h"

enum FaseProses {
  FASE_IDLE, FASE_CEK_LEVEL,
  FASE_DOSING_HCL, FASE_DOSING_H2O2, FASE_DOSING_H2O,
  FASE_JEDA_CAMPUR, FASE_ETCHING, FASE_DRAIN_ETCH,
  FASE_DOSING_SABUN, FASE_BILAS_SABUN, FASE_DRAIN_SABUN,
  FASE_DOSING_AIR, FASE_BILAS_AIR, FASE_DRAIN_AIR,
  FASE_PENDINGINAN, FASE_SELESAI
};

const char* namaFase[] = {
  "IDLE", "CEK LEVEL", "DOSING HCl", "DOSING H2O2", "DOSING H2O",
  "JEDA", "ETCHING", "BUANG ETCHANT", "DOSING SABUN", "BILAS SABUN",
  "BUANG SABUN", "DOSING AIR", "BILAS AIR", "BUANG AIR",
  "PENDINGINAN", "SELESAI"
};

// ===== Durasi estimasi per fase (ms) untuk perhitungan progress =====
// Nilai ini dipakai untuk menghitung totalDurasiMsAktual saat proses mulai
const unsigned long DURASI_FASE_MS[] = {
  0,       // FASE_IDLE
  300,     // FASE_CEK_LEVEL
  30000,   // FASE_DOSING_HCL   (estimasi 30 detik)
  30000,   // FASE_DOSING_H2O2
  30000,   // FASE_DOSING_H2O
  400,     // FASE_JEDA_CAMPUR
  0,       // FASE_ETCHING      (diambil dari p.agitasiDetik)
  15000,   // FASE_DRAIN_ETCH
  3000,    // FASE_DOSING_SABUN
  10000,   // FASE_BILAS_SABUN
  10000,   // FASE_DRAIN_SABUN
  3000,    // FASE_DOSING_AIR
  8000,    // FASE_BILAS_AIR
  10000,   // FASE_DRAIN_AIR
  10000,   // FASE_PENDINGINAN
  0        // FASE_SELESAI
};

bool prosesJalan = false;
bool prosesJeda  = false;
bool dialogBatal = false;
FaseProses prosesFase = FASE_IDLE;

unsigned long faseMulaiMs = 0;
unsigned long faseDurasiMs = 0;
unsigned long prosesAkumMs = 0;
unsigned long prosesTerakhirMs = 0;
int  prosesDetikTerakhir = -1;
bool prosesError = false;

// ===== VARIABEL BARU: total durasi aktual =====
unsigned long prosesTotalMsAktual = 0;   // total durasi semua fase (ms)

bool prosesSelesai() { return prosesFase == FASE_SELESAI; }

// Hitung total durasi semua fase berdasarkan preset
unsigned long hitungTotalDurasiAktual(const Preset &p) {
  unsigned long total = 0;
  for (int i = 0; i < 16; i++) {
    if (i == FASE_ETCHING) {
      total += (unsigned long)p.agitasiDetik * 1000UL;
    } else {
      total += DURASI_FASE_MS[i];
    }
  }
  return total;
}

void prosesSetError(const char* kode, const char* pesan) {
  prosesError = true;
  prosesErrorKode = kode;
  prosesErrorPesan = pesan;
  prosesFase = FASE_IDLE;
  prosesJalan = false;
  aktuatorMatikanSemua();
  fanSet(255);
  buzzerMain(TONE_ERROR);
  pindahHalaman(HAL_ERROR);
}

void faseLanjut(FaseProses berikutnya, unsigned long durasiMs = 0) {
  prosesFase = berikutnya;
  faseMulaiMs = millis();
  faseDurasiMs = durasiMs;
  Serial.print(F("[PROSES] Fase: "));
  Serial.println(namaFase[berikutnya]);
  perluGambar = true;
}

bool faseSelesai() {
  if (faseDurasiMs == 0) return true;
  return (millis() - faseMulaiMs) >= faseDurasiMs;
}

void prosesMulai() {
  if (estopAktif()) {
    prosesSetError("E_STOP", "E-stop aktif. Lepas & reset.");
    return;
  }
  Preset &p = daftarPreset[presetTerpilih];
  int kebutuhan[JUMLAH_SENSOR] = { p.mlHCl, p.mlH2O2, p.mlH2O, p.mlH2O, 0 };
  int volLimbahEstimasi = p.mlHCl + p.mlH2O2 + p.mlH2O + p.mlH2O + p.mlH2O;
  kebutuhan[4] = volLimbahEstimasi;

  int idxKurang = -1;
  if (!cekKetersediaan(kebutuhan, idxKurang)) {
    static char buf[48];
    snprintf(buf, sizeof(buf), "%s kurang", namaSensor[idxKurang]);
    prosesSetError("LEVEL_KURANG", buf);
    return;
  }

  prosesJalan = true;
  prosesJeda = false;
  dialogBatal = false;
  prosesError = false;
  prosesAkumMs = 0;
  prosesTerakhirMs = millis();
  prosesDetikTerakhir = -1;
  
  // ===== BARU: hitung total durasi aktual =====
  prosesTotalMsAktual = hitungTotalDurasiAktual(p);
  Serial.print(F("[PROSES] Total durasi aktual: "));
  Serial.print(prosesTotalMsAktual / 1000);
  Serial.println(F(" s"));

  ledStripSet(true);
  fanSet(200);
  faseLanjut(FASE_CEK_LEVEL, 300);
  Serial.print(F("[PROSES] Mulai: "));
  Serial.println(p.nama);
}

void prosesToggleJeda() {
  if (!prosesJalan || prosesSelesai()) return;
  prosesJeda = !prosesJeda;
  if (prosesJeda) {
    agitasiSet(0);
    stepperSleep(true);
  } else {
    stepperSleep(false);
  }
  perluGambar = true;
}

void prosesBatal() {
  prosesJalan = false;
  prosesJeda = false;
  dialogBatal = false;
  prosesFase = FASE_IDLE;
  aktuatorMatikanSemua();
  fanSet(200);
  perluGambar = true;
}

void prosesKeluarSelesai() {
  prosesJalan = false;
  prosesFase = FASE_IDLE;
  aktuatorMatikanSemua();
  perluGambar = true;
}

void prosesUpdate() {
  if (!prosesJalan) return;
  if (prosesJeda) return;

  if (estopAktif()) {
    prosesSetError("E_STOP", "E-stop ditekan");
    return;
  }

  unsigned long now = millis();
  prosesAkumMs += now - prosesTerakhirMs;
  prosesTerakhirMs = now;

  // ===== BARU: gunakan prosesTotalMsAktual untuk detik =====
  int d = prosesAkumMs / 1000;
  if (d != prosesDetikTerakhir) {
    prosesDetikTerakhir = d;
    perluGambar = true;
  }

  Preset &p = daftarPreset[presetTerpilih];

  switch (prosesFase) {
    case FASE_CEK_LEVEL:
      if (faseSelesai()) faseLanjut(FASE_DOSING_HCL, 0);
      break;

    case FASE_DOSING_HCL:
      if (faseDurasiMs == 0) {
        if (stepperMulaiDosis(0, p.mlHCl)) faseDurasiMs = 1;
      }
      if (faseDurasiMs == 1 && !stepperAktif(0)) faseLanjut(FASE_DOSING_H2O2, 0);
      break;

    case FASE_DOSING_H2O2:
      if (faseDurasiMs == 0) {
        if (stepperMulaiDosis(1, p.mlH2O2)) faseDurasiMs = 1;
      }
      if (faseDurasiMs == 1 && !stepperAktif(1)) faseLanjut(FASE_DOSING_H2O, 0);
      break;

    case FASE_DOSING_H2O:
      if (faseDurasiMs == 0) {
        if (stepperMulaiDosis(2, p.mlH2O)) faseDurasiMs = 1;
      }
      if (faseDurasiMs == 1 && !stepperAktif(2)) faseLanjut(FASE_JEDA_CAMPUR, 400);
      break;

    case FASE_JEDA_CAMPUR:
      if (faseSelesai()) {
        agitasiSet(p.agitasiSpeed);
        faseLanjut(FASE_ETCHING, (unsigned long)p.agitasiDetik * 1000UL);
      }
      break;

    case FASE_ETCHING:
      if (faseSelesai()) { 
        agitasiSet(0); 
        pompaLimbahSet(255); 
        faseLanjut(FASE_DRAIN_ETCH, 15000); 
      }
      break;

    case FASE_DRAIN_ETCH:
      if (faseSelesai()) { 
        pompaLimbahSet(0); 
        pompaSabunSet(255); 
        faseLanjut(FASE_DOSING_SABUN, 3000); 
      }
      break;

    case FASE_DOSING_SABUN:
      if (faseSelesai()) { 
        pompaSabunSet(0); 
        agitasiSet(3); 
        faseLanjut(FASE_BILAS_SABUN, 10000); 
      }
      break;

    case FASE_BILAS_SABUN:
      if (faseSelesai()) { 
        agitasiSet(0); 
        pompaLimbahSet(255); 
        faseLanjut(FASE_DRAIN_SABUN, 10000); 
      }
      break;

    case FASE_DRAIN_SABUN:
      if (faseSelesai()) { 
        pompaLimbahSet(0); 
        pompaSabunSet(255); 
        faseLanjut(FASE_DOSING_AIR, 3000); 
      }
      break;

    case FASE_DOSING_AIR:
      if (faseSelesai()) { 
        pompaSabunSet(0); 
        agitasiSet(2); 
        faseLanjut(FASE_BILAS_AIR, 8000); 
      }
      break;

    case FASE_BILAS_AIR:
      if (faseSelesai()) { 
        agitasiSet(0); 
        pompaLimbahSet(255); 
        faseLanjut(FASE_DRAIN_AIR, 10000); 
      }
      break;

    case FASE_DRAIN_AIR:
      if (faseSelesai()) { 
        pompaLimbahSet(0); 
        stepperSleep(true); 
        faseLanjut(FASE_PENDINGINAN, 10000); 
      }
      break;

    case FASE_PENDINGINAN:
      if (faseSelesai()) { 
        faseLanjut(FASE_SELESAI, 0); 
        buzzerMain(TONE_SELESAI); 
      }
      break;

    case FASE_SELESAI:
      // ===== BARU: paksa progress 100% saat selesai =====
      prosesAkumMs = prosesTotalMsAktual;
      prosesJalan = false;
      aktuatorMatikanSemua();
      perluGambar = true;
      break;

    default: break;
  }
}