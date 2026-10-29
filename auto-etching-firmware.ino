/*
  Menu OLED 6 Tombol - v0.5.1
  Perubahan dari v0.4.1:
    - sensor.h: sensor level sekarang baca GPIO langsung (ganti MPR121 I2C)
    - motor.h (baru): driver stepper DRV8825 x3, non-blocking, ml -> langkah
    - dcmotor.h (baru): PWM TB6612FNG x2 (agitasi 15, sabun 16, limbah 17)
    - process.h: fase DOSING/ETCHING/DRAINING/RINSING kini menggerakkan aktuator
    - E-stop (GPIO8): hentikan semua aktuator & batalkan proses
  Perubahan v0.5.1:
    - Exhaust fan DIHAPUS dari driver TB6612FNG: arus kipas melebihi
      kapasitas modul (~1.2 A), jadi kipas disambung langsung ke 12V dan
      menyala terus. GPIO 18 kini bebas, halaman kalibrasi kipas dihapus.
  Perubahan dari v0.3:
    - Tombol KIRI/KANAN ditukar (config.h)
    - sensor.h: baca 6 sensor level cairan (MPR121), rata-rata + histeresis
    - Menu "Level Cairan": nilai, ambang, status ISI/KSG per sensor;
      ENTER = set ambang dari nilai sekarang (tangki KOSONG), KIRI/KANAN = fine tune,
      BACK = simpan/buang ambang (NVS)
  Catatan v0.3:
    - storage.h: preset disimpan di NVS (Preferences) + versi + checksum;
      data korup/versi beda -> otomatis kembali ke default
    - presetops.h: tambah / duplikat / hapus / edit preset + validasi
    - Menu "Konfigurasi Preset": List -> Aksi (Edit/Duplikat/Hapus) -> Edit
      Edit: ATAS/BAWAH pilih field, KIRI/KANAN ubah nilai, ENTER simpan
      %H2O dan semua ml dihitung otomatis dari %HCl, %H2O2, dan Volume
  Library: Wire, Adafruit_GFX, Adafruit_SSD1306, Preferences (ESP32)
*/

#include <Wire.h>
#include "dbg.h"
#include "config.h"
#include "input.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "storage.h"
#include "buzzer.h"
#include "process.h"
#include "ui.h"

unsigned long waktuRenderTerakhir = 0;

#ifdef DEBUG_RENDER
unsigned long dbgWaktu = 0;
int dbgJumlahRender = 0;
#endif

// ================= HELPER KONFIGURASI =================
void simpanDanToast(const char* ok) {
  tampilToast(simpanPreset() ? ok : "Gagal simpan!");
}

void editGeserField(int delta) {
  editField = constrain(editField + delta, 0, JUMLAH_FIELD - 1);
  if (editField < editScroll) editScroll = editField;
  if (editField >= editScroll + EDIT_VISIBLE) editScroll = editField - EDIT_VISIBLE + 1;
}

void mulaiEdit() {
  presetEdit = daftarPreset[cfgIdx];
  editBerubah = false;
  editField = 0;
  editScroll = 0;
  dbg.print(F("[CFG] Edit: "));
  dbg.println(presetEdit.nama);
  pindahHalaman(HAL_CFG_EDIT);
}

void simpanEdit() {
  daftarPreset[cfgIdx] = presetEdit;
  editBerubah = false;
}

// ================= NAVIGASI =================
// ================= HELPER TEST MOTOR =================
// Semua motor dijalankan dari nol: stepper pakai ramp akselerasi,
// DC/agitasi pakai ramp duty (dcmotorUpdate).
void testMotorMulaiSemua() {
  for (int i = 0; i < 3; i++) pompaTestMulai(i);
  dcSetPct(DC_SABUN,  kalibDutyDC[0]);
  dcSetPct(DC_LIMBAH, kalibDutyDC[1]);
  agitasiSet(testAgitSpeed);
}

void testMotorHentikanSemua() {
  pompaStopAll();       // ramp turun
  dcStopAll();          // ramp turun
}

void tanganiTombol(int t) {
  dbg.print(F("[TOMBOL] "));
  dbg.println(namaTombol[t]);
  buzzerKlik();                 // feedback bunyi saat tombol ditekan
  perluGambar = true;

  switch (halaman) {

    case HAL_MENU_UTAMA:
      if (t == P_ATAS)  listGeser(L_UTAMA, -1, JUMLAH_MENU);
      if (t == P_BAWAH) listGeser(L_UTAMA, +1, JUMLAH_MENU);
      if (t == P_ENTER) {
        if (lsSel[L_UTAMA] == 0) {
          listReset(L_PRESET);
          pindahHalaman(HAL_PRESET_LIST);
        } else if (lsSel[L_UTAMA] == 1) {
          listReset(L_CFG);
          pindahHalaman(HAL_CFG_LIST);
        } else if (lsSel[L_UTAMA] == 2) {
          levelMasuk();
          pindahHalaman(HAL_LEVEL);
        } else {
          listReset(L_AKSI);
          pindahHalaman(HAL_KALIB_MAIN);
        }
      }
      break;

    case HAL_PRESET_LIST:
      if (jumlahPreset > 0) {
        if (t == P_ATAS)  listGeser(L_PRESET, -1, jumlahPreset);
        if (t == P_BAWAH) listGeser(L_PRESET, +1, jumlahPreset);
        if (t == P_ENTER) {
          presetTerpilih = lsSel[L_PRESET];
          presetDetailScroll = 0;
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    case HAL_PRESET_DETAIL: {
      int maxScroll = max(0, DETAIL_CONTENT_H - (SCREEN_HEIGHT - HEADER_H));
      if (t == P_ATAS)  presetDetailScroll = max(0, presetDetailScroll - DETAIL_SCROLL_STEP);
      if (t == P_BAWAH) presetDetailScroll = min(maxScroll, presetDetailScroll + DETAIL_SCROLL_STEP);
      if (t == P_ENTER) {
        if (!pompaSiapSemua()) {
          tampilToast("Kalibrasi pompa dulu");
          break;
        }
        prosesMulai(); pindahHalaman(HAL_PROSES);
      }
      if (t == P_BACK)  pindahHalaman(HAL_PRESET_LIST);
      break;
    }

    case HAL_PROSES:
      if (dialogBatal) {
        if (t == P_ENTER) {
          prosesBatal();
          pindahHalaman(HAL_PRESET_DETAIL);
        } else if (t == P_BACK) {
          dialogBatal = false;
          dbg.println(F("[PROSES] Batal dibatalkan (lanjut)"));
        }
      } else if (prosesSelesai()) {
        if (t == P_ENTER || t == P_BACK) {
          prosesKeluarSelesai();
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      } else {
        if (t == P_ENTER) prosesToggleJeda();
        if (t == P_BACK) {
          dialogBatal = true;
          dbg.println(F("[PROSES] Konfirmasi batal ditampilkan"));
        }
      }
      break;

    case HAL_PLACEHOLDER:
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Level Cairan ----------
    case HAL_LEVEL:
      if (t == P_ATAS) {
        levelScroll = max(0, levelScroll - LEVEL_SCROLL_STEP);
        dbg.println(F("[LEVEL] Scroll up"));
      }
      if (t == P_BAWAH) {
        levelScroll += LEVEL_SCROLL_STEP;
        dbg.println(F("[LEVEL] Scroll down"));
      }
      if (t == P_BACK) {
        levelScroll = 0;
        pindahHalaman(HAL_MENU_UTAMA);
      }
      break;

    // ---------- Kalibrasi Main ----------
    case HAL_KALIB_MAIN:
      if (t == P_ATAS)  listGeser(L_AKSI, -1, JUMLAH_MENU_KALIB);
      if (t == P_BAWAH) listGeser(L_AKSI, +1, JUMLAH_MENU_KALIB);
      if (t == P_ENTER) {
        int a = lsSel[L_AKSI];
        if (a == 0) {
          // Motor
          listReset(L_AKSI);
          kalibMotorIdx = 0;
          pindahHalaman(HAL_KALIB_MOTOR_LIST);
        } else if (a == 1) {
          // Sensor Level
          kalibSensorIdx = 0;
          kalibSetStep = 0;
          pindahHalaman(HAL_KALIB_LEVEL);
        } else if (a == 2) {
          // Sensor Suhu
          kalibSuhuIdx = 0;
          kalibSuhuScroll = 0;
          kalibSuhuField = 0;
          pindahHalaman(HAL_KALIB_SUHU);
        } else if (a == 3) {
          // Sensor Udara (MQ-135)
          kalibUdaraSel = 0;
          pindahHalaman(HAL_KALIB_UDARA);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Kalibrasi Motor: Pilih Motor ----------
    case HAL_KALIB_MOTOR_LIST:
      if (t == P_ATAS)  listGeser(L_AKSI, -1, JUMLAH_MENU_MOTOR);
      if (t == P_BAWAH) listGeser(L_AKSI, +1, JUMLAH_MENU_MOTOR);
      if (t == P_ENTER) {
        kalibMotorIdx = lsSel[L_AKSI];
        kalibStepperCursor = 0;
        kalibDCCursor = 0;
        if (kalibMotorIdx <= 2) {
          // Pompa HCl, H2O2, Air (stepper)
          kalibStepperScroll = 0;
          pindahHalaman(HAL_KALIB_STEPPER);
        } else if (kalibMotorIdx <= 4) {
          // Pompa Sabun, Limbah (DC)
          kalibDCScroll = 0;
          pindahHalaman(HAL_KALIB_DC);
        } else if (kalibMotorIdx == 5) {
          // Motor Agitasi (index 5): setting SPD1-3, tanpa kalibrasi
          kalibDCCursor = 0;
          pindahHalaman(HAL_KALIB_AGITASI);
        } else {
          // Test Motor: semua motor (stepper + DC + agitasi)
          testMotorCursor = 0;
          testMotorScroll = 0;
          testMotorHentikanSemua();
          pindahHalaman(HAL_TEST_MOTOR);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_KALIB_MAIN);
      break;

    // ---------- Kalibrasi Stepper (Pompa HCl/H2O2/Air) ----------
    case HAL_KALIB_STEPPER: {
      int idx = kalibMotorIdx;
      if (t == P_BACK) {
        if (pompaAktif(idx)) pompaStop(idx);
        simpanPompa();
        kalibStepperScroll = 0;
        pindahHalaman(HAL_KALIB_MOTOR_LIST);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        int arah = (t == P_BAWAH) ? +1 : -1;
        kalibStepperCursor = (kalibStepperCursor + arah + 4) % 4;
        // Auto-scroll: pastikan baris terpilih sepenuhnya terlihat
        {
          static const int rowTop[4] = { 0, 12, 22, 32 };
          static const int rowH[4]   = { 12, 10, 10, 10 };
          int maxScroll = max(0, KALIB_STEPPER_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
          int visibleH  = AREA_BAWAH - (HEADER_H + 4);
          if (rowTop[kalibStepperCursor] - kalibStepperScroll + rowH[kalibStepperCursor] > visibleH)
            kalibStepperScroll = rowTop[kalibStepperCursor] + rowH[kalibStepperCursor] - visibleH;
          if (rowTop[kalibStepperCursor] - kalibStepperScroll < 0)
            kalibStepperScroll = rowTop[kalibStepperCursor];
          if (kalibStepperScroll > maxScroll) kalibStepperScroll = maxScroll;
          if (kalibStepperScroll < 0) kalibStepperScroll = 0;
        }
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        if (kalibStepperCursor == 1) {
          // RPM live: kalau pompa sedang jalan, target baru ikut ramp.
          pompaAturRpmLive(idx, kalibStepperRpm[idx] + arah * PUMP_RPM_STEP);
        } else if (kalibStepperCursor == 2) {
          // Hasil ukur dalam 0,1 ml, langkah edit bisa 0,1/0,5/1 ml.
          kalibStepperHasilMl10[idx] = constrain(
              kalibStepperHasilMl10[idx] + arah * kalibStepperStep, 0, 9999);
          kalibStepperMl[idx] =
              (kalibStepperHasilMl10[idx] / 10.0f) / (float)PUMP_KALIB_REV;
          dbg.print(F("[KALIB-POMPA] hasil="));
          dbg.print(kalibStepperHasilMl10[idx] / 10.0f);
          dbg.print(F(" ml -> ml/rot="));
          dbg.println(kalibStepperMl[idx]);
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibStepperCursor == 0) {
          if (pompaAktif(idx)) {
            pompaStop(idx);
            kalibStepperUkur = false;
            tampilToast("Dihentikan");
          } else if (pompaPutarRevolusi(idx, PUMP_KALIB_REV)) {
            kalibStepperUkur = true;
            tampilToast("Mengisi...");
          } else {
            tampilToast("Gagal start");
          }
        } else if (kalibStepperCursor == 2) {
          // Ganti langkah edit: 0,1 -> 0,5 -> 1,0 ml
          if      (kalibStepperStep == 1)  kalibStepperStep = 5;
          else if (kalibStepperStep == 5)  kalibStepperStep = 10;
          else                             kalibStepperStep = 1;
          tampilToast("Step hasil");
        }
        break;
      }
      break;
    }

    // ---------- Test Motor: semua motor, START/STOP + ubah nilai live ----------
    case HAL_TEST_MOTOR: {
      if (t == P_BACK) {
        testMotorHentikanSemua();
        simpanPompa();
        pindahHalaman(HAL_KALIB_MOTOR_LIST);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        int arah = (t == P_BAWAH) ? +1 : -1;
        testMotorCursor = (testMotorCursor + arah + TEST_MOTOR_JUMLAH) % TEST_MOTOR_JUMLAH;
        testMotorPastikanTerlihat();
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        if (testMotorCursor >= 1 && testMotorCursor <= 3) {
          int idx = testMotorCursor - 1;
          pompaAturRpmLive(idx, kalibStepperRpm[idx] + arah * PUMP_RPM_STEP);
        } else if (testMotorCursor == 4 || testMotorCursor == 5) {
          int idx = testMotorCursor - 4;       // 0=sabun, 1=limbah
          kalibDutyDC[idx] = constrain(kalibDutyDC[idx] + arah * 5, 0, 100);
          int ch = idx + 1;
          if (dcAktif(ch)) dcSetPct(ch, kalibDutyDC[idx]);   // live, ikut ramp
        } else if (testMotorCursor == 6) {
          testAgitSpeed = constrain(testAgitSpeed + arah, 1, 3);
          if (agitasiAktif()) agitasiSet(testAgitSpeed);
        }
        break;
      }
      if (t == P_ENTER) {
        if (testMotorCursor == 0) {
          // Baris "Semua": nyalakan/hentikan semua motor
          if (testMotorAktifBaris(0)) {
            testMotorHentikanSemua();
            tampilToast("Stop semua");
          } else {
            testMotorMulaiSemua();
            tampilToast("Jalan semua");
          }
        } else if (testMotorCursor <= 3) {
          int idx = testMotorCursor - 1;
          if (pompaAktif(idx)) {
            pompaStop(idx);
            tampilToast("Stop");
          } else if (pompaTestMulai(idx)) {
            tampilToast("Running...");
          } else {
            tampilToast("Gagal start");
          }
        } else if (testMotorCursor <= 5) {
          int idx = testMotorCursor - 4;
          int ch = idx + 1;
          if (dcAktif(ch)) {
            dcStop(ch);
            tampilToast("Stop");
          } else {
            dcSetPct(ch, kalibDutyDC[idx]);
            tampilToast("Running...");
          }
        } else {
          if (agitasiAktif()) {
            agitasiSet(0);
            tampilToast("Stop");
          } else {
            agitasiSet(testAgitSpeed);
            tampilToast("Running...");
          }
        }
        break;
      }
      break;
    }

    // ---------- Kalibrasi DC (Pompa Sabun/Limbah) ----------
    // Uji 10 detik: RAMPUP -> UKUR 10 s -> TURUN; hasil ml diukur setelah
    // pompa benar-benar diam, jadi tidak terpengaruh ramp.
    case HAL_KALIB_DC: {
      int idx = kalibMotorIdx - 3;                 // 0=sabun, 1=limbah
      if (t == P_BACK) {
        if (kalibDcJalan()) kalibDcStop();
        dcStop(idx + 1);
        simpanPompa();
        kalibDCScroll = 0;
        pindahHalaman(HAL_KALIB_MOTOR_LIST);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        int arah = (t == P_BAWAH) ? +1 : -1;
        kalibDCCursor = (kalibDCCursor + arah + 4) % 4;
        // Auto-scroll: pastikan baris terpilih sepenuhnya terlihat
        {
          static const int rowH[4] = { 12, 10, 10, 10 };
          int maxScroll = max(0, KALIB_DC_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
          int visibleH  = AREA_BAWAH - (HEADER_H + 4);
          int top = KALIB_DC_ROW_TOP[kalibDCCursor];
          if (top - kalibDCScroll + rowH[kalibDCCursor] > visibleH)
            kalibDCScroll = top + rowH[kalibDCCursor] - visibleH;
          if (top - kalibDCScroll < 0)
            kalibDCScroll = top;
          if (kalibDCScroll > maxScroll) kalibDCScroll = maxScroll;
          if (kalibDCScroll < 0) kalibDCScroll = 0;
        }
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        if (kalibDCCursor == 1) {
          // Duty live: kalau uji sedang berjalan, mulai ulang siklus
          // (ramp + 10 detik) agar ml/detik tetap sesuai duty terpilih.
          kalibDutyDC[idx] = constrain(kalibDutyDC[idx] + arah * 5, 0, 100);
          if (kalibDcJalan()) kalibDcMulai(idx);
        } else if (kalibDCCursor == 2) {
          // Hasil ukur ml; ml/detik = hasil / KALIB_DC_DETIK.
          kalibDcHasilMl[idx] = constrain(kalibDcHasilMl[idx] + arah * kalibDcStep, 0, 9999);
          kalibDcMlPerSec[idx] =
              (float)kalibDcHasilMl[idx] / (float)KALIB_DC_DETIK;
          dbg.print(F("[KALIB-DC] hasil="));
          dbg.print(kalibDcHasilMl[idx]);
          dbg.print(F(" ml -> ml/det="));
          dbg.println(kalibDcMlPerSec[idx]);
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibDCCursor == 0) {
          if (kalibDcJalan()) {
            kalibDcStop();
            kalibDcUkur = false;
            tampilToast("Dihentikan");
          } else if (kalibDutyDC[idx] <= 0) {
            tampilToast("Duty 0%");
          } else {
            kalibDcUkur = true;
            kalibDcMulai(idx);
            tampilToast("Ukur 10 dtk");
          }
        } else if (kalibDCCursor == 2) {
          // Ganti langkah edit: 1 -> 5 -> 10 ml
          if      (kalibDcStep == 1)  kalibDcStep = 5;
          else if (kalibDcStep == 5)  kalibDcStep = 10;
          else                        kalibDcStep = 1;
          tampilToast("Step hasil");
        }
        break;
      }
      break;
    }

    // ---------- Setting Agitasi: 3 level kecepatan (tanpa kalibrasi ml) ----------
    case HAL_KALIB_AGITASI: {
      if (t == P_BACK) {
        agitasiSet(0);
        simpanPompa();
        pindahHalaman(HAL_KALIB_MOTOR_LIST);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        int arah = (t == P_BAWAH) ? +1 : -1;
        kalibDCCursor = (kalibDCCursor + arah + 3) % 3;
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        agitasiSpdDuty[kalibDCCursor] =
            constrain(agitasiSpdDuty[kalibDCCursor] + arah * 5, 0, 100);
        // Live: kalau SPD ini sedang diuji, target baru ikut diramp.
        if (agitasiAktif() && agitasiSpdUji == kalibDCCursor + 1)
          dcSetPct(DC_AGI, agitasiSpdDuty[kalibDCCursor]);
        break;
      }
      if (t == P_ENTER) {
        int spd = kalibDCCursor + 1;
        if (agitasiAktif() && agitasiSpdUji == spd) {
          agitasiSet(0);
          tampilToast("Stop");
        } else {
          agitasiSpdUji = spd;
          agitasiSet(spd);
          tampilToast("Running...");
        }
        break;
      }
      break;
    }

    // ---------- Kalibrasi Sensor Suhu ----------
    case HAL_KALIB_SUHU:
      if (t == P_BACK) { kalibSuhuScroll = 0; pindahHalaman(HAL_KALIB_MAIN); break; }
      if (t == P_ATAS)  kalibSuhuIdx = max(0, kalibSuhuIdx - 1);
      if (t == P_BAWAH) kalibSuhuIdx = min(JUMLAH_SUHU - 1, kalibSuhuIdx + 1);
      if (t == P_ENTER) kalibSuhuField = 1 - kalibSuhuField;   // pindah field aktif
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        if (kalibSuhuField == 0) {
          // Field addr: pilih sensor 1..3
          kalibSuhuAddr[kalibSuhuIdx] = constrain(kalibSuhuAddr[kalibSuhuIdx] + arah, 1, 3);
          dbg.print(F("[KALIB-SUHU] "));
          dbg.print(namaSensorSuhu[kalibSuhuIdx]);
          dbg.print(F(" addr="));
          dbg.println(kalibSuhuAddr[kalibSuhuIdx]);
        } else {
          // Field suhu: atur offset kalibrasi ±0.1 °C
          suhuOffsetX10[kalibSuhuIdx] += arah * 1.0f;
          if (suhuOffsetX10[kalibSuhuIdx] >  200.0f) suhuOffsetX10[kalibSuhuIdx] =  200.0f;
          if (suhuOffsetX10[kalibSuhuIdx] < -200.0f) suhuOffsetX10[kalibSuhuIdx] = -200.0f;
          dbg.print(F("[KALIB-SUHU] "));
          dbg.print(namaSensorSuhu[kalibSuhuIdx]);
          dbg.print(F(" offset="));
          dbg.println(suhuOffsetX10[kalibSuhuIdx] / 10.0f);
        }
        break;
      }
      // Auto-scroll: pastikan blok terpilih terlihat
      {
        int maxScroll = max(0, SUHU_CONTENT_H - (AREA_BAWAH - HEADER_H - 3));
        int visibleH  = AREA_BAWAH - (HEADER_H + 3);
        if (SUHU_ROW_TOP[kalibSuhuIdx] - kalibSuhuScroll + SUHU_ROW_H > visibleH)
          kalibSuhuScroll = SUHU_ROW_TOP[kalibSuhuIdx] + SUHU_ROW_H - visibleH;
        if (SUHU_ROW_TOP[kalibSuhuIdx] - kalibSuhuScroll < 0)
          kalibSuhuScroll = SUHU_ROW_TOP[kalibSuhuIdx];
        if (kalibSuhuScroll > maxScroll) kalibSuhuScroll = maxScroll;
        if (kalibSuhuScroll < 0) kalibSuhuScroll = 0;
      }
      break;

    // ---------- Kalibrasi Sensor Udara (MQ-135) ----------
    case HAL_KALIB_UDARA:
      if (t == P_BACK) { pindahHalaman(HAL_KALIB_MAIN); break; }
      if (t == P_ATAS || t == P_BAWAH) kalibUdaraSel = 1 - kalibUdaraSel;
      if (t == P_ENTER) {
        // Rekam bacaan saat ini (ppm) sebagai acuan baris terpilih
        mq135Bacaan = sensorUdaraRaw();
        int ppm = (int)mq135PPM();
        if (kalibUdaraSel == 0) {
          mq135Normal = ppm;
          tampilToast("Normal direkam");
        } else {
          mq135Kotor = ppm;
          tampilToast("Kotor direkam");
        }
        dbg.print(F("[KALIB-MQ135] "));
        dbg.print(kalibUdaraSel == 0 ? F("normal") : F("kotor"));
        dbg.print(F(" adc="));
        dbg.print(mq135Bacaan);
        dbg.print(F(" ppm="));
        dbg.println(ppm);
      }
      break;

    // ---------- Kalibrasi Sensor Level: Pilih Cairan ----------
    case HAL_KALIB_LEVEL: {
      const int ROW_H = 10;
      const int VISIBLE = (AREA_BAWAH - HEADER_H) / ROW_H;
      int maxScroll = max(0, JUMLAH_SENSOR - VISIBLE);
      if (t == P_ATAS) {
        kalibSensorIdx = max(0, kalibSensorIdx - 1);
        if (kalibSensorIdx < kalibLevelScroll) kalibLevelScroll = kalibSensorIdx;
      }
      if (t == P_BAWAH) {
        kalibSensorIdx = min(JUMLAH_SENSOR - 1, kalibSensorIdx + 1);
        if (kalibSensorIdx >= kalibLevelScroll + VISIBLE) kalibLevelScroll = kalibSensorIdx - VISIBLE + 1;
      }
      if (t == P_ENTER) {
        kalibSetStep = 0;
        pindahHalaman(HAL_KALIB_LEVEL_SET);
      }
      if (t == P_BACK) {
        kalibLevelScroll = 0;
        pindahHalaman(HAL_KALIB_MAIN);
      }
      break;
    }

    // ---------- Kalibrasi Sensor Level: Set 0% / 100% ----------
    case HAL_KALIB_LEVEL_SET:
      if (t == P_BACK) {
        pindahHalaman(HAL_KALIB_LEVEL);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        kalibSetStep = 1 - kalibSetStep;
        break;
      }
      if (t == P_ENTER) {
        if (kalibSetStep == 0) {
          sensorSetKosong(kalibSensorIdx);
          dbg.print(F("[KALIB] 0% tersimpan: "));
          dbg.println(sensorKalibKosong[kalibSensorIdx]);
          tampilToast("0% tersimpan");
        } else {
          sensorSetPenuh(kalibSensorIdx);
          simpanKalib();
          dbg.print(F("[KALIB] 100% tersimpan: "));
          dbg.println(sensorKalibPenuh[kalibSensorIdx]);
          tampilToast("100% tersimpan");
          pindahHalaman(HAL_KALIB_LEVEL);
        }
      }
      break;

    // ---------- Konfigurasi: daftar preset + "+ Tambah" ----------
    case HAL_CFG_LIST: {
      int n = jumlahItemCfg();
      if (t == P_ATAS)  listGeser(L_CFG, -1, n);
      if (t == P_BAWAH) listGeser(L_CFG, +1, n);
      if (t == P_ENTER) {
        int idx = lsSel[L_CFG];
        if (idx < jumlahPreset) {
          cfgIdx = idx;
          listReset(L_AKSI);
          pindahHalaman(HAL_CFG_AKSI);
        } else if (tambahPreset()) {
          dbg.print(F("[CFG] Tambah: "));
          dbg.println(daftarPreset[jumlahPreset - 1].nama);
          listSet(L_CFG, jumlahPreset - 1);
          simpanDanToast("Preset ditambah");
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;
    }

    // ---------- Konfigurasi: Edit / Duplikat / Hapus ----------
    case HAL_CFG_AKSI:
      if (dialogCfg == DLG_HAPUS) {
        if (t == P_ENTER) {
          dbg.print(F("[CFG] Hapus: "));
          dbg.println(daftarPreset[cfgIdx].nama);
          hapusPreset(cfgIdx);
          dialogCfg = DLG_NONE;
          listSet(L_CFG, min(cfgIdx, jumlahItemCfg() - 1));
          pindahHalaman(HAL_CFG_LIST);
          simpanDanToast("Preset dihapus");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
        }
      } else {
        if (t == P_ATAS)  listGeser(L_AKSI, -1, 3);
        if (t == P_BAWAH) listGeser(L_AKSI, +1, 3);
        if (t == P_ENTER) {
          int a = lsSel[L_AKSI];
          if (a == 0) {
            mulaiEdit();
          } else if (a == 1) {
            if (duplikatPreset(cfgIdx)) {
              dbg.print(F("[CFG] Duplikat -> "));
              dbg.println(daftarPreset[jumlahPreset - 1].nama);
              listSet(L_CFG, jumlahPreset - 1);
              pindahHalaman(HAL_CFG_LIST);
              simpanDanToast("Preset diduplikat");
            } else {
              tampilToast("Penuh (maks 10)");
            }
          } else {
            dialogCfg = DLG_HAPUS;
          }
        }
        if (t == P_BACK) pindahHalaman(HAL_CFG_LIST);
      }
      break;

    // ---------- Konfigurasi: Edit field ----------
    case HAL_CFG_EDIT:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          simpanEdit();
          dialogCfg = DLG_NONE;
          pindahHalaman(HAL_CFG_AKSI);
          simpanDanToast("Tersimpan");
        } else if (t == P_BACK) {
          dbg.println(F("[CFG] Perubahan dibuang"));
          dialogCfg = DLG_NONE;
          editBerubah = false;
          pindahHalaman(HAL_CFG_AKSI);
        }
      } else {
        if (t == P_ATAS)  editGeserField(-1);
        if (t == P_BAWAH) editGeserField(+1);
        if (t == P_KIRI || t == P_KANAN) {
          Preset sebelum = presetEdit;
          ubahField(presetEdit, editField, (t == P_KANAN) ? +1 : -1);
          if (memcmp(&sebelum, &presetEdit, sizeof(Preset)) != 0) editBerubah = true;
        }
        if (t == P_ENTER) {
          simpanEdit();
          simpanDanToast("Tersimpan");
        }
        if (t == P_BACK) {
          if (editBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_CFG_AKSI);
        }
      }
      break;
  }
}

// ================= SETUP =================
void setup() {
  dbgInit();
  delay(300);
  dbg.print(F("[BOOT] Menu_OLED_6Button "));
  dbg.println(F(FW_VERSI));

  muatPreset();
  muatKalib();
  muatPompa();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK);

  if (!tulisPCF8574(0xFF))
    dbg.println(F("!! PCF8574 tidak terdeteksi. Cek wiring/alamat I2C."));
  inputInit();

  if (!sensorInit())
    dbg.println(F("!! Sensor level tidak siap. Cek wiring GPIO."));
  sensorSuhuInit();                      // scan address DS18B20 di GPIO10

  motorInit();
  dcmotorInit();
  buzzerInit();
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_MQ135, INPUT);                 // sensor udara MQ-135
  pinMode(PIN_DS18B20, INPUT);               // sensor suhu (1-Wire, placeholder)

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    dbg.println(F("!! OLED tidak terdeteksi di 0x3C. Cek wiring/alamat I2C."));
  } else {
    oled.setRotation(0);
    gambarLayar();
    perluGambar = false;
  }

  // Port USB baru terhubung setelah host membuka monitor; cetak ulang banner
  // agar pesan boot tidak hilang.
  delay(2000);
  dbgInit();
  dbg.print(F("[BOOT] Menu_OLED_6Button "));
  dbg.println(F(FW_VERSI));

  // Nada boot diputar di akhir setup(): kalau dipanggil lebih awal, melodi
  // akan tertahan oleh delay() di atas (buzzerUpdate baru jalan di loop).
  buzzerBoot();
}

// ================= LOOP =================
void loop() {
  int t = bacaTombolBaru();
  if (t != TIDAK_ADA) tanganiTombol(t);

  prosesUpdate();                            // logika proses, tidak tergantung render
  motorUpdate();                             // stepper non-blocking
  dcmotorUpdate();                           // ramp duty DC/agitasi non-blocking
  kalibDcUpdate();                           // state machine uji DC 10 detik
  sensorUpdate();                            // baca sensor level cairan
  buzzerUpdate();                            // pola nada non-blocking

  // Redraw halaman kalibrasi stepper saat status START/STOP berubah.
  // Saat uji PUMP_KALIB_REV rotasi selesai sendiri, minta user mengukur ml.
  if (halaman == HAL_KALIB_STEPPER) {
    static bool stepperJalanTerakhir = false;
    bool jalan = pompaAktif(kalibMotorIdx);
    if (jalan != stepperJalanTerakhir) {
      stepperJalanTerakhir = jalan;
      perluGambar = true;
      if (!jalan && kalibStepperUkur) {
        kalibStepperUkur = false;
        tampilToast("Selesai, ukur ml");
      }
    }
  }

  // Redraw halaman kalibrasi DC: status uji, hitung mundur, dan selesai.
  if (halaman == HAL_KALIB_DC) {
    static bool dcJalanTerakhir = false;
    static int  dcSisaTerakhir  = -1;
    bool jalan = kalibDcJalan();
    int  sisa  = kalibDcSisaDetik();
    if (jalan != dcJalanTerakhir || sisa != dcSisaTerakhir) {
      dcJalanTerakhir = jalan;
      dcSisaTerakhir  = sisa;
      perluGambar = true;
      if (dcJalanTerakhir == false && kalibDcUkur) {
        kalibDcUkur = false;
        tampilToast("Selesai, ukur ml");
      }
    }
  }

  // Redraw halaman setting agitasi saat relay status berubah.
  if (halaman == HAL_KALIB_AGITASI) {
    static bool agitJalanTerakhir = false;
    bool jalan = agitasiAktif();
    if (jalan != agitJalanTerakhir) { agitJalanTerakhir = jalan; perluGambar = true; }
  }

  // Redraw halaman Test Motor saat status salah satu motor berubah.
  if (halaman == HAL_TEST_MOTOR) {
    static uint8_t testStatusTerakhir = 0;
    uint8_t status = (pompaAktif(0) ? 1 : 0) |
                     (pompaAktif(1) ? 2 : 0) |
                     (pompaAktif(2) ? 4 : 0) |
                     (dcAktif(DC_SABUN) ? 8 : 0) |
                     (dcAktif(DC_LIMBAH) ? 16 : 0) |
                     (dcAktif(DC_AGI) ? 32 : 0);
    if (status != testStatusTerakhir) { testStatusTerakhir = status; perluGambar = true; }
  }

  // Konversi DS18B20 non-blocking + redraw kalibrasi saat data baru tiba
  if (halaman == HAL_KALIB_SUHU) {
    sensorSuhuUpdate();
    if (sensorSuhuSiapTampil()) perluGambar = true;
  }

  // Bacaan real-time untuk halaman kalibrasi MQ-135
  if (halaman == HAL_KALIB_UDARA) {
    static unsigned long udaraMs = 0;
    unsigned long now = millis();
    if (now - udaraMs >= 100) {
      udaraMs = now;
      mq135Bacaan = sensorUdaraRaw();
      perluGambar = true;
    }
  }

  // E-stop: hentikan semua aktuator & batalkan proses
  if (digitalRead(PIN_ESTOP) == LOW) {
    pompaStopAllDarurat();               // potong seketika, tanpa ramp
    dcStopAllDarurat();                  // agitasi + pompa DC, tanpa ramp
    kalibDcFase = KDC_IDLE;
    kalibDcIdx  = -1;
    kalibDcUkur = false;
    kalibStepperUkur = false;
    if (prosesJalan) {
      prosesBatal();
      tampilToast("E-STOP!");
    }
  }

  bool animasiBerjalan = false;
  for (int L = 0; L < JUMLAH_LIST; L++) {
    if (lsAnimasi[L]) animasiBerjalan = true;   // termasuk frame terakhir animasi
    listUpdateAnimasi(L);
  }
  if (animasiBerjalan) perluGambar = true;

  if (toastAktif && (long)(millis() - toastSampai) >= 0) {
    toastAktif = false;
    perluGambar = true;
  }

  unsigned long sekarang = millis();
  if (perluGambar && (sekarang - waktuRenderTerakhir >= INTERVAL_RENDER_MS)) {
    perluGambar = false;                     // sebelum gambar, agar tombol saat display() tidak hilang
    gambarLayar();
    waktuRenderTerakhir = sekarang;
#ifdef DEBUG_RENDER
    dbgJumlahRender++;
#endif
#ifdef DEBUG_LAMBAT_RENDER_MS
    delay(DEBUG_LAMBAT_RENDER_MS);
#endif

    // Baca ulang tombol setelah oled.display() (bagian paling lama memblokir I2C)
    int t2 = bacaTombolBaru();
    if (t2 != TIDAK_ADA) tanganiTombol(t2);
  }

#ifdef DEBUG_RENDER
  if (sekarang - dbgWaktu >= 1000) {
    dbg.print(F("[DBG] render/detik: "));
    dbg.println(dbgJumlahRender);
    dbgJumlahRender = 0;
    dbgWaktu = sekarang;
  }
#endif

  delay(2);
}
