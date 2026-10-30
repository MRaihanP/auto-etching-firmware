#pragma once
#include <Arduino.h>
#include "config.h"

// ================= DATA PRESET =================
// Catatan: semua field int + char[12] -> tanpa padding (aman untuk disimpan sebagai blob)
struct Preset {
  char  nama[12];
  int   pcbPanjang;      // mm
  int   pcbLebar;        // mm
  int   persenHCl;       // %
  int   persenH2O2;      // %
  int   persenH2O;       // % (otomatis = 100 - HCl - H2O2)
  int   mlHCl;
  int   mlH2O2;
  int   mlH2O;
  int   agitasiDetik;    // durasi agitasi per siklus
  int   agitasiSpeed;    // level kecepatan 1-3 (SPD1/SPD2/SPD3)
  int   totalDetik;      // estimasi lama total proses
};

#define MAKS_PRESET    10
#define JUMLAH_DEFAULT 5

// Data default (dipakai saat pertama kali / data korup)
const Preset presetDefault[JUMLAH_DEFAULT] = {
  { "Preset 1", 100, 80, 33, 17, 50, 120,  60, 180,  5, 3, 270 },
  { "Preset 2",  80, 60, 40, 20, 40, 100,  50, 100,  8, 3, 330 },
  { "Preset 3", 150,100, 25, 25, 50, 150, 150, 300,  4, 2, 420 },
  { "Preset 4",  60, 40, 50, 10, 40,  90,  18,  72, 10, 3, 210 },
  { "Preset 5", 120, 90, 30, 20, 50, 135,  90, 225,  6, 3, 360 }
};

Preset daftarPreset[MAKS_PRESET];   // data kerja (dimuat dari NVS saat boot)
int jumlahPreset = 0;

// ================= MENU UTAMA =================
#define JUMLAH_MENU 4
const char* menuUtama[JUMLAH_MENU] = {
  "Pilih Preset",
  "Konfigurasi Preset",
  "Level Cairan",
  "Kalibrasi"
};

// ================= HALAMAN & STATE =================
enum Halaman {
  HAL_MENU_UTAMA,
  HAL_PRESET_LIST,
  HAL_PRESET_DETAIL,
  HAL_PROSES,
  HAL_PLACEHOLDER,
  HAL_CFG_LIST,
  HAL_CFG_AKSI,
  HAL_CFG_EDIT,
  HAL_LEVEL,
  HAL_KALIB_MAIN,
  HAL_KALIB_LEVEL,
  HAL_KALIB_LEVEL_SET,
  HAL_KALIB_MOTOR_LIST,
  HAL_KALIB_STEPPER,
  HAL_KALIB_DC,
  HAL_KALIB_AGITASI,
  HAL_KALIB_SUHU,
  HAL_KALIB_UDARA,
  HAL_TEST_MOTOR
};
const char* namaHalaman[] = {
  "MENU_UTAMA", "PRESET_LIST", "PRESET_DETAIL", "PROSES", "PLACEHOLDER",
  "CFG_LIST", "CFG_AKSI", "CFG_EDIT", "LEVEL", "KALIB_MAIN", "KALIB_LEVEL", "KALIB_LEVEL_SET",
  "KALIB_MOTOR_LIST", "KALIB_STEPPER", "KALIB_DC", "KALIB_AGITASI",
  "KALIB_SUHU", "KALIB_UDARA", "TEST_MOTOR"
};

Halaman halaman = HAL_MENU_UTAMA;
int presetTerpilih = 0;

// Dirty flag: layar hanya digambar ulang kalau true
bool perluGambar = true;

// State scroll Detail Preset
int presetDetailScroll = 0;
const int DETAIL_SCROLL_STEP = 5;
const int DETAIL_CONTENT_H   = 90;   // tinggi total konten (virtual)

// State scroll Level Cairan
int levelScroll = 0;
const int LEVEL_SCROLL_STEP = 5;

// State Kalibrasi Sensor Level
int kalibSensorIdx = 0;
int kalibSetStep = 0;
int kalibLevelScroll = 0;

// State Kalibrasi Motor/Pompa
int kalibMotorIdx = 0;        // 0=HCl, 1=H2O2, 2=Air, 3=Sabun, 4=Limbah, 5=Agitasi, 6=Test Motor
int kalibMotorScroll = 0;
int kalibDCScroll = 0;        // scroll offset untuk halaman DC

// ---- Kalibrasi pompa stepper (HCl/H2O2/Air) ----
// Uji kalibrasi v0.6.4: user memasukkan target ml -> START (dosing memakai
// kalibrasi sekarang, ramp sama seperti Test Motor) -> ukur hasil nyata ->
// isi "Hasil" -> ENTER = hitung offset + kalibrasi ulang.
// Cursor 0=START/STOP, 1=RPM, 2=Target ml, 3=Hasil ukur, 4=Offset, 5=ml/rotasi
int kalibStepperCursor = 0;
int kalibStepperScroll = 0;
int   kalibStepperRpm[3]        = { PUMP_RPM_DEFAULT, PUMP_RPM_DEFAULT, PUMP_RPM_DEFAULT };
float kalibStepperMl[3]         = { 0.0f, 0.0f, 0.0f }; // ml per rotasi (hasil kalibrasi)
int   kalibStepperStep          = 1;                     // langkah edit hasil: 1=0,1 5=0,5 10=1 ml
int   kalibStepperTgtStep       = 5;                     // langkah edit target: 1/5/10 ml
int   kalibStepperTargetMl[3]   = { 10, 10, 10 };        // target uji (ml)
int   kalibStepperUkurMl10[3]   = { 0, 0, 0 };           // hasil ukur nyata (0,1 ml)
int   kalibStepperOffsetX10[3]  = { 0, 0, 0 };           // offset terakhir (0,1 %)
bool  kalibStepperOffsetAda[3]  = { false, false, false };
float kalibStepperCalUsed[3]    = { 0.0f, 0.0f, 0.0f };  // ml/rotasi efektif saat uji
// Verifikasi hasil kalibrasi (baris "Uji hasil"):
// - ujiMode  : run yang sedang/baru dijalankan adalah uji, bukan kalibrasi
// - ujiAda   : sudah ada hasil uji (LULUS/GAGAL)
// - ujiLulus : hasil uji terakhir lulus toleransi
// - koreksiSiap : uji gagal, ENTER di baris Hasil berikutnya = koreksi
bool  kalibStepperUjiMode[3]    = { false, false, false };
bool  kalibStepperUjiAda[3]     = { false, false, false };
bool  kalibStepperUjiLulus[3]   = { false, false, false };
bool  kalibStepperKoreksiSiap[3]= { false, false, false };

// ---- Kalibrasi pompa DC (sabun/limbah) ----
// Cursor 0=START/STOP, 1=Duty, 2=Target ml, 3=Hasil ukur, 4=Offset, 5=ml/detik
int   kalibDCCursor       = 0;
int   kalibDutyDC[2]      = { 80, 100 };   // duty pompa sabun & limbah (%)
float kalibDcMlPerSec[2]  = { 0.0f, 0.0f }; // hasil kalibrasi ml/detik
int   kalibDcStep         = 1;              // langkah edit hasil: 1/5/10 ml
int   kalibDcTgtStep      = 5;              // langkah edit target: 1/5/10 ml
int   kalibDcTargetMl[2]  = { 50, 50 };     // target uji (ml)
int   kalibDcUkurMl[2]    = { 0, 0 };       // hasil ukur nyata (ml)
int   kalibDcOffsetX10[2] = { 0, 0 };       // offset terakhir (0,1 %)
bool  kalibDcOffsetAda[2] = { false, false };
float kalibDcCalUsed[2]   = { 0.0f, 0.0f }; // ml/detik efektif saat uji
unsigned long kalibDcDurasiMs = 0;          // durasi uji target ml yang sedang berjalan
// Verifikasi hasil kalibrasi DC (baris "Uji hasil").
bool  kalibDcUjiMode[2]     = { false, false };
bool  kalibDcUjiAda[2]      = { false, false };
bool  kalibDcUjiLulus[2]    = { false, false };
bool  kalibDcKoreksiSiap[2] = { false, false };

// Status uji DC (dipakai dcmotor.h)
enum KalibDcFase { KDC_IDLE, KDC_RAMPUP, KDC_UKUR, KDC_TURUN };
KalibDcFase   kalibDcFase = KDC_IDLE;
int           kalibDcIdx  = -1;
unsigned long kalibDcT0   = 0;
bool          kalibDcUkur = false;   // true = uji berjalan, beri toast saat selesai
bool          kalibStepperUkur = false; // true = uji stepper berjalan (toast selesai)

// ---- Motor agitasi: 3 level kecepatan, tanpa kalibrasi ----
int agitasiSpdDuty[3] = { AGITASI_SPD1_DEFAULT, AGITASI_SPD2_DEFAULT, AGITASI_SPD3_DEFAULT };
int agitasiSpdUji     = 1;   // SPD yang sedang diuji di halaman agitasi (1..3)

// ---- Halaman Test Motor ----
// 0=Semua, 1=HCl, 2=H2O2, 3=Air, 4=Sabun, 5=Limbah, 6=Agitasi
int testMotorCursor  = 0;
int testMotorScroll  = 0;
int testAgitSpeed    = 1;   // SPD agitasi yang dipakai saat test (1..3)

// State Kalibrasi Sensor Suhu (3 blok)
#define JUMLAH_SUHU 3
const char* namaSensorSuhu[JUMLAH_SUHU] = {
  "Suhu Motor", "Suhu Komponen", "Suhu Chamber"
};
int kalibSuhuIdx    = 0;       // blok terpilih
int kalibSuhuScroll = 0;       // scroll offset halaman suhu
int kalibSuhuField  = 0;       // 0 = addr sensor, 1 = suhu saat ini
const int SUHU_ROW_TOP[3]  = { 0, 26, 52 };   // offset dari HEADER_H+3 (gap 4px antar blok)
const int SUHU_ROW_H       = 22;              // tinggi tiap blok
const int SUHU_CONTENT_H   = SUHU_ROW_TOP[2] + SUHU_ROW_H + 3;

// State Kalibrasi Sensor Udara (MQ-135)
int kalibUdaraSel    = 0;     // 0 = normal, 1 = kotor
int mq135Normal      = 0;     // acuan udara bersih (ppm)
int mq135Kotor       = 0;     // acuan udara kotor (ppm)
int mq135Bacaan      = 0;     // ADC real-time (diupdate di loop)

// State Konfigurasi Preset
enum DialogCfg { DLG_NONE, DLG_SIMPAN, DLG_HAPUS };
DialogCfg dialogCfg = DLG_NONE;
int    cfgIdx = 0;            // preset yang sedang di-edit/diaksi
Preset presetEdit;            // salinan kerja saat edit
bool   editBerubah = false;
int    editField = 0;
int    editScroll = 0;

void pindahHalaman(Halaman h) {
  dbg.print(F("[NAV] "));
  dbg.print(namaHalaman[halaman]);
  dbg.print(F(" -> "));
  dbg.println(namaHalaman[h]);
  halaman = h;
  perluGambar = true;
}
