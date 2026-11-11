#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "dbg.h"

// ================= DATA PRESET (v3) =================
// Semua field int + char[12] -> tanpa padding (aman untuk blob NVS).
// Perubahan v2 (v1.0.0):
//   - totalDetik DIHAPUS (dihitung dari model kalibrasi)
//   - siklus, bilasSabun, bilasAir, bilasMl, bilasDetik, konfirmasiSiklus
//   - agitasiDetik = lama etching/agitasi PER SIKLUS (30..1800 s)
// Perubahan v3 (v1.1.0):
//   - rasio kimia bisa diisi persen ATAU ml (modeRasio)
//   - waktu etching bisa otomatis dari tebal tembaga + laju etch (modeWaktu)
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
  int   agitasiDetik;    // lama etching per siklus (30..1800 s)
  int   agitasiSpeed;    // 1..3 (SPD1/SPD2/SPD3)
  int   siklus;          // jumlah siklus penuh 1..6 (dosing+etching+bilas)
  int   bilasSabun;      // jumlah bilas sabun 0..3
  int   bilasAir;        // jumlah bilas air 0..5
  int   bilasMl;         // volume tiap bilas (ml)
  int   bilasDetik;      // agitasi tiap bilas (s)
  int   konfirmasiSiklus; // 0/1
  int   modeRasio;       // 0 = persen, 1 = ml
  int   isiTembagaPct;   // estimasi luas tembaga / luas PCB (%)
  int   tebalTembagaUm;  // tebal foil tembaga (um), 1 oz = 35
  int   lajuEtchX10;     // laju etching (0,1 um/menit), mis. 30 = 3,0
  int   modeWaktu;       // 0 = manual agitasiDetik, 1 = otomatis dari PCB
};

#define MAKS_PRESET    10
#define JUMLAH_DEFAULT 5

// Data default (dipakai saat pertama kali / data korup).
// Rasio kimia dipertahankan seperti v0.6.6 (33:17:50) sampai pengguna
// mengonfirmasi rasio baru; field siklus/bilas memakai nilai aman awal.
const Preset presetDefault[JUMLAH_DEFAULT] = {
  { "Preset 1", 100, 80, 33, 17, 50, 120,  60, 180, 300, 3, 2, 1, 2, 100, 20, 0, 0, 40, 35, ETCH_LAJU_DEFAULT_X10, 1 },
  { "Preset 2",  80, 60, 40, 20, 40, 100,  50, 100, 240, 3, 3, 1, 2, 100, 20, 0, 0, 40, 35, ETCH_LAJU_DEFAULT_X10, 1 },
  { "Preset 3", 150,100, 25, 25, 50, 150, 150, 300, 420, 2, 2, 1, 1, 120, 30, 0, 0, 40, 35, ETCH_LAJU_DEFAULT_X10, 1 },
  { "Preset 4",  60, 40, 50, 10, 40,  90,  18,  72, 360, 3, 2, 0, 2,  80, 15, 0, 0, 40, 35, ETCH_LAJU_DEFAULT_X10, 1 },
  { "Preset 5", 120, 90, 30, 20, 50, 135,  90, 225, 330, 3, 2, 1, 2, 100, 20, 0, 0, 40, 35, ETCH_LAJU_DEFAULT_X10, 1 }
};

Preset daftarPreset[MAKS_PRESET];   // data kerja (dimuat dari NVS saat boot)
int jumlahPreset = 0;

// ================= PENGATURAN GLOBAL =================
struct Pengaturan {
  int tempPeringatanX10;   // °C x10
  int tempBahayaX10;       // °C x10
  int drainMarginPct;      // %
  int levelStabilMs;
  int mqAktif;             // 0/1
  int iotEnabled;          // 0/1 (v1.1)
  int kapHclMl;            // kapasitas jerigen sumber
  int kapH2o2Ml;
  int kapH2oMl;
  int kapSabunMl;
  int kapLimbahMl;
};
Pengaturan pengaturan = {
  (int)(TEMP_PERINGATAN_C * 10.0f),
  (int)(TEMP_BAHAYA_C * 10.0f),
  DRAIN_MARGIN_PCT,
  LEVEL_STABIL_MS,
  1,
  0,
  KAPASITAS_HCL_ML,
  KAPASITAS_H2O2_ML,
  KAPASITAS_H2O_ML,
  KAPASITAS_SABUN_ML,
  TANGKI_MAKS_ML
};

// ================= PELACAK TANGKI (ml) =================
// Sisa volume di tangki sumber + isi tangki limbah. Nilai ini dipakai
// PRECHECK untuk memastikan seluruh proses tercukupi.
long tankHclMl   = 0;
long tankH2o2Ml  = 0;
long tankH2oMl   = 0;
long tankSabunMl = 0;
long tankLimbahMl = 0;
bool tankDataAda  = false;   // data NVS tangki pernah diisi

int kapasitasTank(int idx) {
  switch (idx) {
    case 0: return pengaturan.kapHclMl;
    case 1: return pengaturan.kapH2o2Ml;
    case 2: return pengaturan.kapH2oMl;
    case 3: return pengaturan.kapSabunMl;
    case 4: return pengaturan.kapLimbahMl;
    default: return 0;
  }
}

void clampTankKeKapasitas() {
  tankHclMl    = constrain(tankHclMl,    0L, (long)pengaturan.kapHclMl);
  tankH2o2Ml   = constrain(tankH2o2Ml,   0L, (long)pengaturan.kapH2o2Ml);
  tankH2oMl    = constrain(tankH2oMl,    0L, (long)pengaturan.kapH2oMl);
  tankSabunMl  = constrain(tankSabunMl,  0L, (long)pengaturan.kapSabunMl);
  tankLimbahMl = constrain(tankLimbahMl, 0L, (long)pengaturan.kapLimbahMl);
}

// ================= MENU UTAMA =================
#define JUMLAH_MENU 8
const char* menuUtama[JUMLAH_MENU] = {
  "Pilih Preset",
  "Konfigurasi Preset",
  "Level Cairan",
  "Kalibrasi",
  "Pengaturan",
  "Isi Ulang Tangki",
  "Status IoT",
  "Info/Status"
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
  HAL_KALIB_ESTOP,
  HAL_TEST_MOTOR,
  HAL_PENGATURAN,
  HAL_TANK,
  HAL_TANK_EDIT,
  HAL_IOT_STATUS,
  HAL_INFO
};
const char* namaHalaman[] = {
  "MENU_UTAMA", "PRESET_LIST", "PRESET_DETAIL", "PROSES", "PLACEHOLDER",
  "CFG_LIST", "CFG_AKSI", "CFG_EDIT", "LEVEL", "KALIB_MAIN", "KALIB_LEVEL", "KALIB_LEVEL_SET",
  "KALIB_MOTOR_LIST", "KALIB_STEPPER", "KALIB_DC", "KALIB_AGITASI",
  "KALIB_SUHU", "KALIB_UDARA", "KALIB_ESTOP", "TEST_MOTOR",
  "PENGATURAN", "TANK", "TANK_EDIT", "IOT_STATUS", "INFO"
};

Halaman halaman = HAL_MENU_UTAMA;
int presetTerpilih = 0;

// Dirty flag: layar hanya digambar ulang kalau true
bool perluGambar = true;
bool sensorDataBerubah = false;   // event flag untuk konsumen non-UI (API)
bool otaAktif = false;            // true selama upload OTA berlangsung
int  otaHasilTerakhir = 0;       // 0 belum, 1 sukses, 2 error (persist NVS)

// ================= EVENT LOG (ring buffer) =================
// Timestamp memakai millis(). Setelah NTP tersedia, epoch bisa ditambahkan
// di lapisan IoT tanpa mengubah logika proses.
#define EVENT_MAX 32
struct EventRec {
  uint32_t ms;
  uint32_t epoch;   // 0 sampai NTP tersinkron
  uint16_t kode;
  char     detail[24];
};
EventRec eventLog[EVENT_MAX];
int eventHead = 0;
int eventCount = 0;

void eventCatat(uint16_t kode, const char* detail) {
  EventRec &e = eventLog[eventHead];
  e.ms = millis();
  e.epoch = (uint32_t)time(nullptr);
  e.kode = kode;
  if (detail != nullptr) {
    strncpy(e.detail, detail, sizeof(e.detail) - 1);
    e.detail[sizeof(e.detail) - 1] = '\0';
  } else {
    e.detail[0] = '\0';
  }
  eventHead = (eventHead + 1) % EVENT_MAX;
  if (eventCount < EVENT_MAX) eventCount++;
}

// State scroll Detail Preset
int presetDetailScroll = 0;
const int DETAIL_SCROLL_STEP = 5;
const int DETAIL_CONTENT_H   = 120;   // tinggi total konten (virtual)

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
// Alur kalibrasi 100 ml: target tetap 100 ml. START memakai model saat ini (atau base
// nominal bila belum ada), lalu hasil ukur dimasukkan di baris "Hasil".
// Koreksi: a = langkah_diperintahkan / hasil_ml, b = 0.
// Cursor 0=START, 1=Priming, 2=RPM, 3=Target, 4=Hasil, 5=Koreksi,
//        6=Run, 7=Reset
int kalibStepperCursor = 0;
int kalibStepperScroll = 0;
int   kalibStepperRpm[3]        = { PUMP_RPM_DEFAULT, PUMP_RPM_DEFAULT, PUMP_RPM_DEFAULT };
float kalibStepperMl[3]         = { 0.0f, 0.0f, 0.0f };   // ml/rotasi (diturunkan dari a)
float kalibStepperA[3]          = { 0.0f, 0.0f, 0.0f };   // langkah per ml
float kalibStepperB[3]          = { 0.0f, 0.0f, 0.0f };   // offset langkah
// Titik uji dipertahankan sebagai data legacy/persistensi. Alur 100 ml tidak
// lagi menambah titik; model disimpan langsung di a/b.
int   kalibStepperTitikN[3]     = { 0, 0, 0 };
float kalibStepperTitikMl[3][6];
float kalibStepperTitikStep[3][6];
int   kalibStepperStep          = 10;                   // langkah edit hasil (0,1 ml): 1=0,1 10=1 50=5 ml
int   kalibStepperUkurMl10[3]   = { 0, 0, 0 };          // hasil ukur nyata (0,1 ml)
int   kalibStepperOffsetX10[3]  = { 0, 0, 0 };          // koreksi terakhir (0,1 %)
bool  kalibStepperOffsetAda[3]  = { false, false, false };
long  kalibStepperStepUsed[3]   = { 0, 0, 0 };          // langkah yang diperintahkan run terakhir
int   kalibStepperHasilTerakhir[3] = { 0, 0, 0 };       // hasil ukur terakhir yang diterapkan (0,1 ml)
long  kalibStepperStepTerakhir[3]  = { 0, 0, 0 };       // langkah run terakhir yang diterapkan
int   kalibStepperRunKe[3]      = { 0, 0, 0 };          // run sesi ini (RAM, reset saat masuk halaman)
bool  kalibStepperLulus[3]      = { false, false, false };// koreksi terakhir <= toleransi
// 0-2 stepper, 3-4 DC, 5 agitasi, 6 level, 7 suhu, 8 MQ-135
uint32_t kalibEpoch[9]          = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
bool  kalibStepperKoreksiBesar[3] = { false, false, false };// menunggu ENTER kedua
bool  kalibStepperPriming[3]    = { false, false, false };// pompa sedang priming
bool  kalibStepperCekGelas[3]   = { false, false, false };// START berikutnya minta konfirmasi gelas kosong
unsigned long kalibStepperPrimeMs[3] = { 0, 0, 0 };      // waktu mulai priming (millis)

// ---- Kalibrasi pompa DC (sabun/limbah) ----
// Model v1.0.0: ml = r * detik + c, dibangun dari titik (durasi, volume).
// Cursor 0=START/STOP, 1=Duty, 2=Target ml, 3=Hasil ukur, 4=Offset,
//        5=r/c (ml/detik, ml), 6=Run status, 7=Reset kalib
int   kalibDCCursor       = 0;
int   kalibDutyDC[2]      = { 80, 100 };   // duty pompa sabun & limbah (%)
float kalibDcMlPerSec[2]  = { 0.0f, 0.0f }; // r (ml/detik)
float kalibDcC[2]         = { 0.0f, 0.0f }; // c (ml)
int   kalibDcTitikN[2]    = { 0, 0 };
float kalibDcTitikDetik[2][6];
float kalibDcTitikMl[2][6];
int   kalibDcStep         = 1;              // langkah edit hasil: 1 ml
int   kalibDcTgtStep      = 5;              // langkah edit target: 1/5/10 ml
int   kalibDcTargetMl[2]  = { 100, 100 };   // target uji (ml)
int   kalibDcUkurMl[2]    = { 0, 0 };       // hasil ukur nyata (ml)
int   kalibDcOffsetX10[2] = { 0, 0 };       // offset terakhir (0,1 %)
bool  kalibDcOffsetAda[2] = { false, false };
float kalibDcCalUsed[2]   = { 0.0f, 0.0f }; // r efektif saat uji
unsigned long kalibDcDurasiMs = 0;          // durasi perintah run yang sedang berjalan
float kalibDcDetikAktual      = 0.0f;       // durasi perintah run terakhir (termasuk ramp)
// Sesi kalibrasi baru: run 1 = pulse pendek, run 2+ = target ml.
int   kalibDcRunKe[2]     = { 0, 0 };
bool  kalibDcLulus[2]     = { false, false };
bool  kalibDcPulseRun[2]  = { false, false };
bool  kalibDcUjiAda[2]    = { false, false };  // hasil run terakhir sudah diterapkan
int   kalibDcModelDuty[2] = { -1, -1 };        // duty saat model r dibuat

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
int mq135PPMTerkini  = 0;     // ppm hasil konversi terakhir

// Pilihan pada layar proses yang dijeda.
int jedaSel = 0;

// State Konfigurasi Preset
enum DialogCfg { DLG_NONE, DLG_SIMPAN, DLG_HAPUS };
DialogCfg dialogCfg = DLG_NONE;
int    cfgIdx = 0;            // preset yang sedang di-edit/diaksi
Preset presetEdit;            // salinan kerja saat edit
bool   editBerubah = false;
int    editField = 0;
int    editScroll = 0;

// ================= STATE PROSES (dipakai UI + storage runstate) =================
enum FaseProses {
  FASE_IDLE,
  FASE_PRECHECK,
  FASE_WAIT_PCB,
  FASE_ETCH,
  FASE_BILAS_SABUN,
  FASE_BILAS_AIR,
  FASE_DRAIN_BATAL,
  FASE_PEMULIHAN,
  FASE_SELESAI
};
enum LangkahProses {
  LANGKAH_NONE = 0,
  L_DOSE_AIR,
  L_DOSE_H2O2,
  L_DOSE_HCL,
  L_AGITASI,
  L_DRAIN,
  L_DECIDE,
  L_KONFIRMASI,
  L_ISI,
  L_BILAS_AGITASI
};

// ================= PENGATURAN: STATE UI =================
int settingCursor = 0;
int tankEditIdx   = 0;
int tankEditStep  = 50;
int tankScroll    = 0;
int iotScroll     = 0;
int infoScroll    = 0;
const char* infoAlasanReset = "-";

void pindahHalaman(Halaman h) {
  dbg.print(F("[NAV] "));
  dbg.print(namaHalaman[halaman]);
  dbg.print(F(" -> "));
  dbg.println(namaHalaman[h]);
  halaman = h;
  perluGambar = true;
}
