#pragma once
#include <Arduino.h>

// ================= DATA PRESET =================
struct Preset {
  char  nama[12];
  int   pcbPanjang;
  int   pcbLebar;
  int   persenHCl;
  int   persenH2O2;
  int   persenH2O;
  int   mlHCl;
  int   mlH2O2;
  int   mlH2O;
  int   agitasiDetik;
  int   agitasiSpeed;
  int   totalDetik;
  uint8_t modeDosing;    // 0 = auto (47/24/29), 1 = manual
  uint8_t modeAgitasi;   // 0 = auto, 1 = manual
};

#define MAKS_PRESET    10
#define JUMLAH_DEFAULT 5

const Preset presetDefault[JUMLAH_DEFAULT] = {
  { "Preset 1", 100, 80, 47, 24, 29, 80,  40,  50,  60, 3, 270, 0, 0 },
  { "Preset 2",  80, 60, 47, 24, 29, 60,  30,  37,  80, 4, 330, 0, 0 },
  { "Preset 3", 150,100, 47, 24, 29, 100, 51,  62,  60, 2, 420, 0, 0 },
  { "Preset 4",  60, 40, 47, 24, 29, 50,  25,  31, 100, 5, 210, 0, 0 },
  { "Preset 5", 120, 90, 47, 24, 29, 80,  40,  50,  80, 3, 360, 0, 0 }
};

Preset daftarPreset[MAKS_PRESET];
int jumlahPreset = 0;
int presetTerakhirAktif = 0;

// ================= KALIBRASI SENSOR LEVEL (multi-titik) =================
struct KalibrasiSensor {
  int16_t titik[KALIBRASI_TITIK];   // raw count di 0/25/50/75/100%
  int16_t kapasitasMl;
};

KalibrasiSensor kalibrasiSensor[JUMLAH_SENSOR];
bool kalibrasiSensorValid[JUMLAH_SENSOR];

// ================= KALIBRASI AKTUATOR & SENSOR LAIN =================
struct KalibrasiData {
  // Stepper: ml per step x1000 (untuk presisi desimal)
  int32_t stepperMlPerStepX1000[3];  // [0]=HCl, [1]=H2O2, [2]=H2O
  
  // MQ-135: baseline di udara normal + margin toleransi
  int16_t mq135Baseline;
  int16_t mq135Margin;
  
  // DS18B20: offset suhu x10 (untuk presisi 0.1°C)
  int16_t ds18b20OffsetX10;
  
  // Motor DC Agitasi: duty cycle minimum untuk mulai bergerak
  uint8_t agitasiDutyMin;
};

KalibrasiData kalibrasi;

// ================= MENU UTAMA (5 item sesuai dokumen) =================
#define JUMLAH_MENU 5
const char* menuUtama[JUMLAH_MENU] = {
  "Pilih Preset",
  "Buat Preset",
  "Level Cairan",
  "Kalibrasi",
  "Tentang"
};

// ================= MENU KALIBRASI (5 sub-menu) =================
#define JUMLAH_MENU_KALIB 5
const char* menuKalibrasi[JUMLAH_MENU_KALIB] = {
  "Sensor Level",
  "Stepper (ml/step)",
  "MQ-135 (baseline)",
  "DS18B20 (offset)",
  "Agitasi (duty min)"
};

// ================= HALAMAN & STATE =================
enum Halaman {
  HAL_SPLASH,
  HAL_MENU_UTAMA,
  HAL_PRESET_LIST,
  HAL_PRESET_DETAIL,
  HAL_PROSES,
  HAL_PLACEHOLDER,
  HAL_CFG_LIST,
  HAL_CFG_AKSI,
  HAL_CFG_EDIT,
  HAL_CADANGAN,
  HAL_KALIBRASI,        // menu utama kalibrasi
  HAL_KALIB_SENSOR,     // kalibrasi sensor level (5 titik)
  HAL_KALIB_STEPPER,    // kalibrasi stepper
  HAL_KALIB_MQ135,      // kalibrasi MQ-135
  HAL_KALIB_DS18B20,    // kalibrasi DS18B20
  HAL_KALIB_AGITASI,    // kalibrasi agitasi
  HAL_TENTANG,
  HAL_ERROR
};

const char* namaHalaman[] = {
  "SPLASH", "MENU_UTAMA", "PRESET_LIST", "PRESET_DETAIL", "PROSES",
  "PLACEHOLDER", "CFG_LIST", "CFG_AKSI", "CFG_EDIT", "CADANGAN",
  "KALIBRASI", "KALIB_SENSOR", "KALIB_STEPPER", "KALIB_MQ135", 
  "KALIB_DS18B20", "KALIB_AGITASI", "TENTANG", "ERROR"
};

Halaman halaman = HAL_SPLASH;
int presetTerpilih = 0;
bool perluGambar = true;

int presetDetailScroll = 0;
const int DETAIL_SCROLL_STEP = 10;   // = BARIS_H (ui.h)
const int DETAIL_CONTENT_H   = 68;    // tinggi konten: 13..81 (logical)

enum DialogCfg { DLG_NONE, DLG_SIMPAN, DLG_HAPUS };
DialogCfg dialogCfg = DLG_NONE;
int    cfgIdx = 0;
Preset presetEdit;
bool   editBerubah = false;
int    editField = 0;
int    editScroll = 0;

// State kalibrasi sensor level
int kalibSensorSel = 0;
int kalibSensorTitik = -1;   // -1 = baris "Kapasitas"; 0..4 = titik 0%..100%
bool kalibSensorBerubah = false;

// State wizard kalibrasi stepper: test pompa → input volume → hitung ml/step
enum KStepMode { KSTEP_IDLE, KSTEP_INPUT };
KStepMode kstepMode = KSTEP_IDLE;
int kstepInputMl = 250;

// State kalibrasi aktuator
int kalibSel = 0;
int kalibStepperIdx = 0;
int kalibStepperStep = 100;
bool kalibBerubah = false;

const char* prosesErrorKode = "";
const char* prosesErrorPesan = "";

void pindahHalaman(Halaman h) {
  Serial.print(F("[NAV] "));
  Serial.print(namaHalaman[halaman]);
  Serial.print(F(" -> "));
  Serial.println(namaHalaman[h]);
  halaman = h;
  perluGambar = true;
}