#include <Wire.h>
#include "config.h"
#include "input.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "storage.h"
#include "aktuator.h"
#include "process.h"
#include "ui.h"

unsigned long waktuRenderTerakhir = 0;
unsigned long splashMulaiMs = 0;

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
  pindahHalaman(HAL_CFG_EDIT);
}

void simpanEdit() {
  daftarPreset[cfgIdx] = presetEdit;
  editBerubah = false;
}

void tanganiTombol(int t) {
  Serial.print(F("[TOMBOL] "));
  Serial.println(namaTombol[t]);
  buzzerMain(TONE_TOMBOL);
  perluGambar = true;

  switch (halaman) {
    case HAL_SPLASH: break;

    case HAL_MENU_UTAMA:
      if (t == P_ATAS)  listGeser(L_UTAMA, -1, JUMLAH_MENU);
      if (t == P_BAWAH) listGeser(L_UTAMA, +1, JUMLAH_MENU);
      if (t == P_ENTER) {
        switch (lsSel[L_UTAMA]) {
          case 0: listReset(L_PRESET); pindahHalaman(HAL_PRESET_LIST); break;
          case 1: listReset(L_CFG);    pindahHalaman(HAL_CFG_LIST);    break;
          case 2: levelSel = 0; pindahHalaman(HAL_CADANGAN);  break;
          case 3: listReset(L_KALIB); pindahHalaman(HAL_KALIBRASI); break;
          case 4: pindahHalaman(HAL_TENTANG);   break;
        }
      }
      break;

    case HAL_PRESET_LIST:
      if (jumlahPreset > 0) {
        if (t == P_ATAS)  listGeser(L_PRESET, -1, jumlahPreset);
        if (t == P_BAWAH) listGeser(L_PRESET, +1, jumlahPreset);
        if (t == P_ENTER) {
          presetTerpilih = lsSel[L_PRESET];
          presetTerakhirAktif = presetTerpilih;
          presetDetailScroll = 0;
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    case HAL_PRESET_DETAIL: {
      int maxScroll = max(0, DETAIL_CONTENT_H - (AREA_BAWAH - HEADER_H));
      if (t == P_ATAS)  presetDetailScroll = max(0, presetDetailScroll - DETAIL_SCROLL_STEP);
      if (t == P_BAWAH) presetDetailScroll = min(maxScroll, presetDetailScroll + DETAIL_SCROLL_STEP);
      if (t == P_ENTER) { prosesMulai(); if (!prosesError) pindahHalaman(HAL_PROSES); }
      if (t == P_BACK)  pindahHalaman(HAL_PRESET_LIST);
      break;
    }

    case HAL_PROSES:
      if (dialogBatal) {
        if (t == P_ENTER) { prosesBatal(); pindahHalaman(HAL_PRESET_DETAIL); }
        else if (t == P_BACK) { dialogBatal = false; }
      } else if (prosesSelesai()) {
        if (t == P_ENTER || t == P_BACK) {
          prosesKeluarSelesai();
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      } else {
        if (t == P_ENTER) prosesToggleJeda();
        if (t == P_BACK) dialogBatal = true;
      }
      break;

    case HAL_PLACEHOLDER:
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ATAS/BAWAH: scroll baris; KIRI/KANAN: pilih baris
    case HAL_CADANGAN:
      // ATAS/BAWAH scroll per 1 baris cairan (BARIS_LVL, bukan BARIS_H)
      if (t == P_ATAS)  uiScrollAtur(-1, BARIS_LVL);
      if (t == P_BAWAH) uiScrollAtur(+1, BARIS_LVL);
      // KIRI/KANAN pindah baris terpilih; layar ikut ter-scroll
      if (t == P_KIRI)  levelSel = max(0, levelSel - 1);
      if (t == P_KANAN) levelSel = min(JUMLAH_SENSOR - 1, levelSel + 1);
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Menu Kalibrasi ----------
    case HAL_KALIBRASI:
      if (t == P_ATAS)  listGeser(L_KALIB, -1, JUMLAH_MENU_KALIB);
      if (t == P_BAWAH) listGeser(L_KALIB, +1, JUMLAH_MENU_KALIB);
      if (t == P_ENTER) {
        switch (lsSel[L_KALIB]) {
          case 0: kalibSensorSel = 0; kalibSensorTitik = -1; pindahHalaman(HAL_KALIB_SENSOR); break;
          case 1: kalibStepperIdx = 0; pindahHalaman(HAL_KALIB_STEPPER); break;
          case 2: pindahHalaman(HAL_KALIB_MQ135); break;
          case 3: pindahHalaman(HAL_KALIB_DS18B20); break;
          case 4: pindahHalaman(HAL_KALIB_AGITASI); break;
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Kalibrasi Sensor Level ----------
    // ATAS/BAWAH  : pilih baris (Kapasitas / titik 0%..100%)
    // KIRI/KANAN  : ubah kapasitas (baris Kapasitas) / ganti senyawa (baris titik)
    // ENTER       : simpan titik = raw saat ini
    case HAL_KALIB_SENSOR: {
      static char buf[32];
      if (t == P_ATAS)  kalibSensorTitik = constrain(kalibSensorTitik - 1, -1, KALIBRASI_TITIK - 1);
      if (t == P_BAWAH) kalibSensorTitik = constrain(kalibSensorTitik + 1, -1, KALIBRASI_TITIK - 1);
      if (t == P_KIRI) {
        if (kalibSensorTitik < 0) {
          kalibrasiSensor[kalibSensorSel].kapasitasMl =
            constrain(kalibrasiSensor[kalibSensorSel].kapasitasMl - 50, 100, 5000);
          kalibSensorBerubah = true;
        } else {
          kalibSensorSel = max(0, kalibSensorSel - 1);
        }
      }
      if (t == P_KANAN) {
        if (kalibSensorTitik < 0) {
          kalibrasiSensor[kalibSensorSel].kapasitasMl =
            constrain(kalibrasiSensor[kalibSensorSel].kapasitasMl + 50, 100, 5000);
          kalibSensorBerubah = true;
        } else {
          kalibSensorSel = min(JUMLAH_SENSOR - 1, kalibSensorSel + 1);
        }
      }
      if (t == P_ENTER) {
        if (kalibSensorTitik < 0) {
          snprintf(buf, sizeof(buf), "Kapasitas %d ml",
                   kalibrasiSensor[kalibSensorSel].kapasitasMl);
          tampilToast(buf);
        } else {
          kalibrasiSensor[kalibSensorSel].titik[kalibSensorTitik] = sensorAvg[kalibSensorSel];
          kalibSensorBerubah = true;
          if (kalibSensorTitik == KALIBRASI_TITIK - 1) {
            // Titik 100% terakhir → tandai senyawa ini terkalibrasi
            kalibrasiSensorValid[kalibSensorSel] = true;
            snprintf(buf, sizeof(buf), "%s terkalibrasi", namaSensor[kalibSensorSel]);
            tampilToast(buf);
          } else {
            tampilToast("Titik diset");
          }
        }
      }
      if (t == P_BACK) {
        if (kalibSensorBerubah) dialogCfg = DLG_SIMPAN;
        else pindahHalaman(HAL_KALIBRASI);
      }
      break;
    }

    // ---------- Kalibrasi Stepper ----------
    // Alur: pilih pompa → ENTER (jalan N step) → input volume terukur
    //       → ml/step = volume / step, otomatis tersimpan
    case HAL_KALIB_STEPPER: {
      static char buf[32];
      if (t == P_ATAS)  kalibStepperIdx = max(0, kalibStepperIdx - 1);
      if (t == P_BAWAH) kalibStepperIdx = min(2, kalibStepperIdx + 1);
      if (t == P_KIRI) {
        if (kstepMode == KSTEP_INPUT) kstepInputMl = constrain(kstepInputMl - 5, 5, 9999);
        else kalibStepperStep = max(10, kalibStepperStep - 10);
      }
      if (t == P_KANAN) {
        if (kstepMode == KSTEP_INPUT) kstepInputMl = constrain(kstepInputMl + 5, 5, 9999);
        else kalibStepperStep = min(1000, kalibStepperStep + 10);
      }
      if (t == P_ENTER) {
        if (kstepMode == KSTEP_INPUT) {
          // Simpan hasil: ml/step = volume / jumlah step
          float per = (float)kstepInputMl / (float)kalibStepperStep;
          int32_t perX1000 = (int32_t)(per * 1000.0f + 0.5f);
          if (perX1000 < 1) perX1000 = 1;
          kalibrasi.stepperMlPerStepX1000[kalibStepperIdx] = perX1000;
          stepper[kalibStepperIdx].mlPerStep = perX1000 / 1000.0f;
          kalibBerubah = true;
          kstepMode = KSTEP_IDLE;
          snprintf(buf, sizeof(buf), "%.3f ml/step", perX1000 / 1000.0f);
          tampilToast(buf);
        } else {
          stepperTest(kalibStepperIdx, kalibStepperStep);
          kstepInputMl = 100;
          kstepMode = KSTEP_INPUT;
          tampilToast("Masukkan volume");
        }
      }
      if (t == P_BACK) {
        if (kstepMode == KSTEP_INPUT) {
          kstepMode = KSTEP_IDLE;   // batal input, kembali ke daftar pompa
        } else if (kalibBerubah) {
          dialogCfg = DLG_SIMPAN;
        } else {
          pindahHalaman(HAL_KALIBRASI);
        }
      }
      break;
    }

    // ---------- Kalibrasi MQ-135 ----------
    case HAL_KALIB_MQ135:
      if (t == P_ATAS)  uiScrollAtur(-1);
      if (t == P_BAWAH) uiScrollAtur(+1);
      if (t == P_KIRI)  kalibrasi.mq135Margin = max(5, kalibrasi.mq135Margin - 5);
      if (t == P_KANAN) kalibrasi.mq135Margin = min(100, kalibrasi.mq135Margin + 5);
      if (t == P_ENTER) {
        kalibrasi.mq135Baseline = mq135Baca();
        kalibBerubah = true;
        tampilToast("Baseline diset");
      }
      if (t == P_BACK) {
        if (kalibBerubah) dialogCfg = DLG_SIMPAN;
        else pindahHalaman(HAL_KALIBRASI);
      }
      break;

    // ---------- Kalibrasi DS18B20 ----------
    case HAL_KALIB_DS18B20:
      if (t == P_ATAS)  uiScrollAtur(-1);
      if (t == P_BAWAH) uiScrollAtur(+1);
      if (t == P_KIRI)  kalibrasi.ds18b20OffsetX10 = max(-50, kalibrasi.ds18b20OffsetX10 - 1);
      if (t == P_KANAN) kalibrasi.ds18b20OffsetX10 = min(50, kalibrasi.ds18b20OffsetX10 + 1);
      if (t == P_BACK) {
        if (kalibBerubah) dialogCfg = DLG_SIMPAN;
        else pindahHalaman(HAL_KALIBRASI);
      }
      break;

    // ---------- Kalibrasi Agitasi ----------
    case HAL_KALIB_AGITASI:
      if (t == P_ATAS)  uiScrollAtur(-1);
      if (t == P_BAWAH) uiScrollAtur(+1);
      if (t == P_KIRI)  kalibrasi.agitasiDutyMin = max(10, kalibrasi.agitasiDutyMin - 5);
      if (t == P_KANAN) kalibrasi.agitasiDutyMin = min(200, kalibrasi.agitasiDutyMin + 5);
      if (t == P_ENTER) {
        agitasiSet(map(kalibrasi.agitasiDutyMin, 0, 255, 0, 5));
        delay(2000);
        agitasiSet(0);
        kalibBerubah = true;
        tampilToast("Test selesai");
      }
      if (t == P_BACK) {
        if (kalibBerubah) dialogCfg = DLG_SIMPAN;
        else pindahHalaman(HAL_KALIBRASI);
      }
      break;

    case HAL_TENTANG:
      if (t == P_ATAS)  uiScrollAtur(-1);
      if (t == P_BAWAH) uiScrollAtur(+1);
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

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
          listSet(L_CFG, jumlahPreset - 1);
          simpanDanToast("Preset ditambah");
        }
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;
    }

    case HAL_CFG_AKSI:
      if (dialogCfg == DLG_HAPUS) {
        if (t == P_ENTER) {
          hapusPreset(cfgIdx);
          dialogCfg = DLG_NONE;
          listSet(L_CFG, min(cfgIdx, jumlahItemCfg() - 1));
          pindahHalaman(HAL_CFG_LIST);
          simpanDanToast("Preset dihapus");
        } else if (t == P_BACK) dialogCfg = DLG_NONE;
      } else {
        if (t == P_ATAS)  listGeser(L_AKSI, -1, 3);
        if (t == P_BAWAH) listGeser(L_AKSI, +1, 3);
        if (t == P_ENTER) {
          int a = lsSel[L_AKSI];
          if (a == 0) mulaiEdit();
          else if (a == 1) {
            if (duplikatPreset(cfgIdx)) {
              listSet(L_CFG, jumlahPreset - 1);
              pindahHalaman(HAL_CFG_LIST);
              simpanDanToast("Preset diduplikat");
            } else tampilToast("Penuh (maks 10)");
          } else dialogCfg = DLG_HAPUS;
        }
        if (t == P_BACK) pindahHalaman(HAL_CFG_LIST);
      }
      break;

    case HAL_CFG_EDIT:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          simpanEdit();
          dialogCfg = DLG_NONE;
          pindahHalaman(HAL_CFG_AKSI);
          simpanDanToast("Tersimpan");
        } else if (t == P_BACK) {
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

    case HAL_ERROR:
      if (t == P_ENTER) {
        estopReset();
        if (!estopAktif()) {
          prosesError = false;
          pindahHalaman(HAL_MENU_UTAMA);
        }
      }
      break;
  }

  // Simpan kalibrasi dari semua halaman kalibrasi
  bool diHalKalib = (halaman == HAL_KALIB_SENSOR || halaman == HAL_KALIB_STEPPER ||
                     halaman == HAL_KALIB_MQ135 || halaman == HAL_KALIB_DS18B20 ||
                     halaman == HAL_KALIB_AGITASI);
  
  if (diHalKalib && dialogCfg == DLG_SIMPAN) {
    if (t == P_ENTER) {
      // Update mlPerStep stepper dari kalibrasi sebelum simpan
      for (int i = 0; i < 3; i++) {
        stepper[i].mlPerStep = kalibrasi.stepperMlPerStepX1000[i] / 1000.0f;
        if (stepper[i].mlPerStep <= 0) {
          stepper[i].mlPerStep = STEPPER_ML_PER_REV / (200.0f * STEPPER_MICROSTEP);
        }
      }
      simpanKalibrasi();
      kalibSensorBerubah = false;
      kalibBerubah = false;
      dialogCfg = DLG_NONE;
      pindahHalaman(HAL_KALIBRASI);
      tampilToast("Kalibrasi tersimpan");
    } else if (t == P_BACK) {
      dialogCfg = DLG_NONE;
      pindahHalaman(HAL_KALIBRASI);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.print(F("[BOOT] Menu_OLED_6Button "));
  Serial.println(F(FW_VERSI));

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK);

  muatPreset();
  muatKalibrasi();

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("!! OLED tidak terdeteksi"));
  } else {
    oled.setRotation(0);
  }

  halaman = HAL_SPLASH;
  gambarLayar();
  splashMulaiMs = millis();

  aktuatorInit();

  if (!tulisPCF8574(0xFF))
    Serial.println(F("!! PCF8574 tidak terdeteksi"));
  inputInit();

  if (!sensorInit())
    Serial.println(F("!! Sensor level tidak terdeteksi"));

  while (millis() - splashMulaiMs < 1500) { sensorUpdate(); delay(10); }

  pindahHalaman(HAL_MENU_UTAMA);
  buzzerMain(TONE_BOOT);
}

void loop() {
  int t = bacaTombolBaru();
  if (t != TIDAK_ADA) tanganiTombol(t);

  stepperUpdateAll();  // ← PENTING: update stepper non-blocking
  prosesUpdate();
  sensorUpdate();

  bool animasiBerjalan = false;
  for (int L = 0; L < JUMLAH_LIST; L++) {
    if (lsAnimasi[L]) animasiBerjalan = true;
    listUpdateAnimasi(L);
  }
  if (animasiBerjalan) perluGambar = true;

  if (toastAktif && (long)(millis() - toastSampai) >= 0) {
    toastAktif = false;
    perluGambar = true;
  }

  unsigned long sekarang = millis();
  if (perluGambar && (sekarang - waktuRenderTerakhir >= INTERVAL_RENDER_MS)) {
    perluGambar = false;
    gambarLayar();
    waktuRenderTerakhir = sekarang;

    int t2 = bacaTombolBaru();
    if (t2 != TIDAK_ADA) tanganiTombol(t2);
  }
  delay(2);
}