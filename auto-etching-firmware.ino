/*
  Menu OLED 6 Tombol - v0.5.0
  Perubahan dari v0.4.1:
    - sensor.h: sensor level sekarang baca GPIO langsung (ganti MPR121 I2C)
    - motor.h (baru): driver stepper DRV8825 x3, non-blocking, ml -> langkah
    - dcmotor.h (baru): PWM TB6612FNG x2 (agitasi 15, sabun 16, limbah 17, fan 18)
    - process.h: fase DOSING/ETCHING/DRAINING/RINSING kini menggerakkan aktuator
    - E-stop (GPIO8): hentikan semua aktuator & batalkan proses
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
#include "sensor_extra.h"
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
          listReset(L_KALIB);
          placeholderJudul = "";
          pindahHalaman(HAL_KALIB_LIST);
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
      if (t == P_ENTER) { prosesMulai(); pindahHalaman(HAL_PROSES); }
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

    // ---------- Level Cairan (READ-ONLY: monitoring isi jerigen) ----------
    case HAL_LEVEL:
      // ATAS/BAWAH pindah baris (halaman scroll sendiri, scroll ikut)
      if (t == P_ATAS)  levelSel = max(0, levelSel - 1);
      if (t == P_BAWAH) levelSel = min(JUMLAH_SENSOR - 1, levelSel + 1);
      // ENTER: hanya info cepat, TIDAK membuka halaman lain.
      // Tidak ada pengaturan di sini - semua ada di Kalibrasi -> Sensor Level.
      if (t == P_ENTER && sensorOk) {
        char ib[24];
        snprintf(ib, sizeof(ib), "raw %d  d%d",
                 sensorRaw[levelSel], sensorDelta[levelSel]);
        tampilToast(ib);
      }
      if (t == P_BACK) pindahHalaman(HAL_MENU_UTAMA);
      break;

    // ---------- Menu Kalibrasi ----------
    case HAL_KALIB_LIST:
      if (t == P_ATAS)  listGeser(L_KALIB, -1, JUMLAH_MENU_KALIB);
      if (t == P_BAWAH) listGeser(L_KALIB, +1, JUMLAH_MENU_KALIB);
      if (t == P_ENTER) {
        kalibIdx = lsSel[L_KALIB];
        if (kalibIdx == 0) {
          listReset(L_KALIB_SENSOR);
          pindahHalaman(HAL_KALIB_SENSOR_LIST);
        } else if (kalibIdx >= 3 && kalibIdx <= 5) {
          // Motor HCl / H2O2 / Air -> pilih pompa stepper
          listReset(L_KALIB_POMPA);
          pindahHalaman(HAL_KALIB_POMPA_LIST);
        } else if (kalibIdx == 6 || kalibIdx == 7) {
          // Motor Air Sabun / Limbah -> pilih pompa DC
          listReset(L_KALIB_DC);
          pindahHalaman(HAL_KALIB_DC_LIST);
        } else {
          switch (kalibIdx) {
            case 1:  pindahHalaman(HAL_KALIB_SUHU);    break;
            case 2:  pindahHalaman(HAL_KALIB_UDARA);   break;
            case 8:  pindahHalaman(HAL_KALIB_AGITASI); break;
            default: pindahHalaman(HAL_KALIB_FAN);     break;   // 9 = Exhaust Fan
          }
        }
      }
      if (t == P_BACK) {
        placeholderJudul = "";
        pindahHalaman(HAL_MENU_UTAMA);
      }
      break;

    // ---------- Pilih tangki untuk dikalibrasi ----------
    case HAL_KALIB_SENSOR_LIST:
      if (t == P_ATAS)  listGeser(L_KALIB_SENSOR, -1, JUMLAH_SENSOR);
      if (t == P_BAWAH) listGeser(L_KALIB_SENSOR, +1, JUMLAH_SENSOR);
      if (t == P_ENTER) {
        kalibSensorIdx = lsSel[L_KALIB_SENSOR];
        kalibBaris     = 0;
        kalibBerubah   = false;
        pindahHalaman(HAL_KALIB_SENSOR);
      }
      if (t == P_BACK) pindahHalaman(HAL_KALIB_LIST);
      break;

    // ---------- Kalibrasi Sensor Level ----------
    case HAL_KALIB_SENSOR:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
          tampilToast(simpanAmbang() ? "Kalibrasi tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatAmbang();              // buang perubahan, kembali ke NVS
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
        }
      } else if (kalibSampling) {
        // Sedang mengambil sampel: abaikan semua tombol (kecuali BACK)
        if (t == P_BACK) kalibSampling = false;
      } else {
        if (t == P_ATAS)  kalibBaris = max(0, kalibBaris - 1);
        if (t == P_BAWAH) kalibBaris = min(KAL_BARIS - 1, kalibBaris + 1);

        if (sensorOk) {
          // KIRI/KANAN: ubah nilai baris terpilih
          if (t == P_KIRI || t == P_KANAN) {
            int arah = (t == P_KANAN) ? +1 : -1;
            if (kalibBaris == 0) {                       // Kapasitas (ml)
              sensorSetKapasitas(kalibSensorIdx,
                                 sensorKapasitasMl[kalibSensorIdx] + arah * 50);
              kalibBerubah = true;
              char tb[16];
              snprintf(tb, sizeof(tb), "%d ml", sensorKapasitasMl[kalibSensorIdx]);
              tampilToast(tb);
            } else if (kalibBaris == 2 && sensorKalibValid[kalibSensorIdx]) {
              // Delta 100%: koreksi manual kalau hasil test meleset
              sensorDeltaPenuh[kalibSensorIdx] = constrain(
                  sensorDeltaPenuh[kalibSensorIdx] + arah * 10, TOUCH_DELTA_MIN, 100000);
              kalibBerubah = true;
              sensorHitungPersen(kalibSensorIdx);
              char tb[16];
              snprintf(tb, sizeof(tb), "d=%d", sensorDeltaPenuh[kalibSensorIdx]);
              tampilToast(tb);
            }
          }

          // ENTER: tandai titik acuan (rata-rata 40 sampel ~ 2 detik)
          if (t == P_ENTER && kalibBaris > 0) {
            sensorKalibMulai(kalibSensorIdx, kalibBaris == 1 ? KALIB_NOL : KALIB_PENUH);
            if (kalibSampling) kalibBerubah = true;   // hasilnya nanti ikut disimpan
            else tampilToast("Tidak bisa mulai");
          }
        }

        if (t == P_BACK) {
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_LIST);
        }
      }
      break;

    // ---------- Pilih pompa stepper (HCl / H2O2 / Air) ----------
    case HAL_KALIB_POMPA_LIST:
      if (t == P_ATAS)  listGeser(L_KALIB_POMPA, -1, 3);
      if (t == P_BAWAH) listGeser(L_KALIB_POMPA, +1, 3);
      if (t == P_ENTER) {
        kalibPompaIdx   = lsSel[L_KALIB_POMPA];
        kalibPompaBaris = 0;
        kalibBerubah    = false;
        pindahHalaman(HAL_KALIB_POMPA);
      }
      if (t == P_BACK) pindahHalaman(HAL_KALIB_LIST);
      break;

    // ---------- Kalibrasi pompa stepper ----------
    case HAL_KALIB_POMPA:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_POMPA_LIST);
          tampilToast(simpanKalib() ? "Tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatKalib();
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_POMPA_LIST);
        }
      } else {
        if (t == P_KIRI || t == P_KANAN) {
          int arah = (t == P_KANAN) ? +1 : -1;
          long v = langkahPerMlX1000[kalibPompaIdx] / 1000 + arah;
          if (v < 1) v = 1;
          langkahPerMlX1000[kalibPompaIdx] = v * 1000;
          pompaTerapkanKalibrasi();
          kalibBerubah = true;
          char tb[16];
          snprintf(tb, sizeof(tb), "%ld langkah/ml", v);
          tampilToast(tb);
        }
        // Logika test otomatis belum ada (dijadwalkan terpisah)
        if (t == P_ENTER) tampilToast("Test: belum");
        if (t == P_BACK) {
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_POMPA_LIST);
        }
      }
      break;

    // ---------- Pilih pompa DC (Air Sabun / Limbah) ----------
    case HAL_KALIB_DC_LIST:
      if (t == P_ATAS)  listGeser(L_KALIB_DC, -1, 2);
      if (t == P_BAWAH) listGeser(L_KALIB_DC, +1, 2);
      if (t == P_ENTER) {
        kalibDcIdx   = lsSel[L_KALIB_DC];
        kalibDcBaris = 0;
        kalibBerubah = false;
        pindahHalaman(HAL_KALIB_DC);
      }
      if (t == P_BACK) pindahHalaman(HAL_KALIB_LIST);
      break;

    // ---------- Kalibrasi pompa DC ----------
    case HAL_KALIB_DC:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_DC_LIST);
          tampilToast(simpanKalib() ? "Tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatKalib();
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_DC_LIST);
        }
      } else {
        if (t == P_KIRI || t == P_KANAN) {
          int arah = (t == P_KANAN) ? +1 : -1;
          pompaDcMlPerDtkX10[kalibDcIdx] = constrain(
              pompaDcMlPerDtkX10[kalibDcIdx] + arah * 5, 0, 3000);
          kalibBerubah = true;
          char tb[16];
          snprintf(tb, sizeof(tb), "%d.%d ml/dtk",
                   pompaDcMlPerDtkX10[kalibDcIdx] / 10,
                   pompaDcMlPerDtkX10[kalibDcIdx] % 10);
          tampilToast(tb);
        }
        if (t == P_ENTER) tampilToast("Test: belum");
        if (t == P_BACK) {
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_DC_LIST);
        }
      }
      break;

    // ---------- Kalibrasi Motor Agitasi ----------
    case HAL_KALIB_AGITASI:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
          tampilToast(simpanKalib() ? "Tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatKalib();
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
        }
      } else {
        if (t == P_KIRI || t == P_KANAN) {
          int arah = (t == P_KANAN) ? +1 : -1;
          agitasiDutyMin = constrain(agitasiDutyMin + arah * 5, 0, 255);
          kalibBerubah  = true;
          if (kalibAgitasiNyala) agitasiSetDuty(agitasiDutyMin);
          char tb[16];
          snprintf(tb, sizeof(tb), "duty %d", agitasiDutyMin);
          tampilToast(tb);
        }
        if (t == P_ENTER) {
          kalibAgitasiNyala = !kalibAgitasiNyala;
          agitasiSetDuty(kalibAgitasiNyala ? agitasiDutyMin : 0);
        }
        if (t == P_BACK) {
          if (kalibAgitasiNyala) { kalibAgitasiNyala = false; agitasiSetDuty(0); }
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_LIST);
        }
      }
      break;

    // ---------- Kalibrasi Exhaust Fan ----------
    case HAL_KALIB_FAN:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
          tampilToast(simpanKalib() ? "Tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatKalib();
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
        }
      } else {
        if (t == P_KIRI || t == P_KANAN) {
          int arah = (t == P_KANAN) ? +1 : -1;
          fanPwmMin = constrain(fanPwmMin + arah * 5, 0, 255);
          kalibBerubah = true;
          if (kalibFanNyala) fanSetAt(fanPwmMin);
          char tb[16];
          snprintf(tb, sizeof(tb), "pwm %d", fanPwmMin);
          tampilToast(tb);
        }
        if (t == P_ENTER) {
          kalibFanNyala = !kalibFanNyala;
          fanSetAt(kalibFanNyala ? fanPwmMin : 0);
        }
        if (t == P_BACK) {
          if (kalibFanNyala) { kalibFanNyala = false; fanSetAt(0); }
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_LIST);
        }
      }
      break;

    // ---------- Kalibrasi Sensor Udara (MQ-135) ----------
    case HAL_KALIB_UDARA:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
          tampilToast(simpanKalib() ? "Tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatKalib();
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
        }
      } else {
        if (kalibUdaraBaris < 2 && (t == P_KIRI || t == P_KANAN)) {
          int arah = (t == P_KANAN) ? +1 : -1;
          if (kalibUdaraBaris == 0) mq135Baseline = constrain(mq135Baseline + arah * 5, 0, 4095);
          else                     mq135Margin   = constrain(mq135Margin + arah * 5, 0, 4095);
          kalibBerubah = true;
        }
        // ENTER di baris Baseline: tetapkan bacaan sekarang sebagai acuan
        if (t == P_ENTER && kalibUdaraBaris == 0) {
          mq135Baseline = constrain(udaraBaca(), 0, 4095);
          kalibBerubah = true;
          char tb[16];
          snprintf(tb, sizeof(tb), "base %d", mq135Baseline);
          tampilToast(tb);
        }
        if (t == P_ATAS)  kalibUdaraBaris = max(0, kalibUdaraBaris - 1);
        if (t == P_BAWAH) kalibUdaraBaris = min(1, kalibUdaraBaris + 1);
        if (t == P_BACK) {
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_LIST);
        }
      }
      break;

    // ---------- Kalibrasi Sensor Suhu (DS18B20) ----------
    case HAL_KALIB_SUHU:
      if (dialogCfg == DLG_SIMPAN) {
        if (t == P_ENTER) {
          dialogCfg = DLG_NONE;
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
          tampilToast(simpanKalib() ? "Tersimpan" : "Gagal simpan!");
        } else if (t == P_BACK) {
          dialogCfg = DLG_NONE;
          muatKalib();
          kalibBerubah = false;
          pindahHalaman(HAL_KALIB_LIST);
        }
      } else {
        if (t == P_KIRI || t == P_KANAN) {
          int arah = (t == P_KANAN) ? +1 : -1;
          ds18b20OffsetX10 = constrain(ds18b20OffsetX10 + arah, -500, 500);
          kalibBerubah = true;
          char tb[16];
          snprintf(tb, sizeof(tb), "offset %d.%d C",
                   ds18b20OffsetX10 / 10, abs(ds18b20OffsetX10) % 10);
          tampilToast(tb);
        }
        if (t == P_BACK) {
          if (kalibBerubah) dialogCfg = DLG_SIMPAN;
          else pindahHalaman(HAL_KALIB_LIST);
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
  muatAmbang();
  muatKalib();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK);

  if (!tulisPCF8574(0xFF))
    dbg.println(F("!! PCF8574 tidak terdeteksi. Cek wiring/alamat I2C."));
  inputInit();

  if (!sensorInit())
    dbg.println(F("!! Sensor level tidak siap. Cek wiring GPIO."));
  suhuInit();
  udaraInit();

  motorInit();
  dcmotorInit();
  buzzerInit();
  pinMode(PIN_ESTOP, INPUT_PULLUP);

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
  sensorUpdate();                            // baca sensor level cairan
  buzzerUpdate();                            // pola nada non-blocking

  // E-stop: hentikan semua aktuator & batalkan proses
  if (digitalRead(PIN_ESTOP) == LOW) {
    if (prosesJalan) {
      prosesBatal();
      tampilToast("E-STOP!");
    }
    agitasiSet(0);
    pompaLimbahSet(false);
    pompaSabunSet(false);
    fanSet(false);
    pompaStopAll();
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
