/*
  Alat Etching PCB Otomatis - firmware standalone v1.2.0
  -------------------------------------------------------
  Perubahan v1.2.0 dari v1.1.0:
    - Drain batal/E-stop memakai ledger volume nampan + residu + purge;
      durasi mengikuti cairan yang sudah masuk, bukan waktu tetap.
    - Layar jeda & pemulihan punya opsi Lanjutkan / Batalkan + drain /
      Abaikan (stop). Abaikan menghentikan proses tanpa drain dan kembali
      ke menu, tidak melanjutkan proses.
    - PRECHECK menghitung kebutuhan seluruh proses per jerigen dan ruang
      limbah; setelah lulus, level kering/hampir penuh hanya jadi peringatan.
    - Kapasitas jerigen/tangki bisa diatur di Pengaturan; menu Isi Ulang
      Tangki kembali dengan aksi isi penuh/kosongkan.
    - Pesan fault menyebut item, kebutuhan vs ketersediaan, dan tindakan.
    - Tone buzzer khusus: notifikasi, jeda, resume, stop, batal, error.
    - Layout layar peringatan dirapikan agar teks tidak bertumpuk.

  Perubahan v1.1.0 dari v1.0.4:
    - Preset v3: rasio kimia bisa diisi persen ATAU ml; waktu etching bisa
      otomatis dari tebal tembaga, luas tembaga PCB, dan laju etch terkalibrasi.
    - PRECHECK memakai kapasitas jerigen (HCl/H2O2/H2O 1000 ml, sabun 600 ml)
      untuk total kebutuhan seluruh proses termasuk bilas x siklus.
    - Peringatan dini bila muat tembaga melebihi kapasitas larutan.
    - Migrasi NVS cfg v2 -> v3 otomatis (mode manual agar perilaku lama tetap).
    - Layar proses menampilkan progres dosing (ml sudah / target) untuk
      HCl, H2O2, air, dan sabun; redraw 250 ms saat proses berjalan.
    - E06 palsu saat drain diperbaiki: "penuh" limbah butuh >=85% selama
      3 detik + masa tenggang 2 detik setelah pompa drain menyala.
    - Volume dosis dicatat ke runstate tepat setelah cairan masuk; cancel/
      E-stop mengurangi dosis yang belum terpompa dan sisa drain yang sudah
      berjalan. Drain memakai volume nyata + batas bawah laju 3 ml/s.
      Dialog drain punya opsi BACK=Kosong.
    - Tebal tembaga di preset diedit/ditampilkan dalam oz (0,5..6,0 oz).
    - Menu Kalibrasi -> E-Stop menampilkan status tombol + GPIO8 live.
    - Fondasi IoT: `api.h` (StatusSnapshot, perintahProses, event log,
      command Serial non-blocking; lihat IOT-API.md).
    - MQTT Phase 2: `net.h` — WiFi/ESP32MQTTClient background task, TLS 8883
      dengan CA ISRG Root X2, topik `pelarut/<deviceId>/...`, command queue +
      ACK, provisioning Serial, NTP/epoch, remote config tank/setting/preset
      (IDLE-only), replay 5 event terakhir, rate limit + audit. Default WiFi
      PCB Expres V3.
    - OTA HTTP ringan: `ota.h` — POST /update di port 3232, password
      dari `secrets.h`, hanya saat IDLE/aman; partisi dual-app 4 MB via
      partitions.csv/`PartitionScheme=min_spiffs`. Panduan test di
      PANDUAN-TEST-IOT.md. Provisioning aman/rollback OTA belum.
    - Menu `Status IoT`: WiFi/SSID/IP/RSSI, MQTT, host/port/TLS, device,
      base topic, NTP, hasil OTA terakhir (NVS), dan free heap; scroll
      ATAS/BAWAH.
    - Build perlu `--libraries /home/raihanP/Arduino/libraries` untuk
      ESP32MQTTClient + ArduinoJson.

  Perubahan v1.0.1 dari v1.0.0:
    - Kalibrasi DC: run pertama tiap sesi = pulse pendek (sabun 5 s,
      limbah 3 s), run berikutnya target 100 ml; model dikoreksi langsung
      r = hasil / detik perintah (c = 0); LULUS butuh minimal 3 run.
    - Siklus proses kini mengulang dosing + etching + bilas sabun/air
      sebanyak `siklus` di preset (bilas dihitung per siklus).
    - Cancel proses mematikan semua motor lalu langsung menyalakan pompa
      limbah; E-stop/ERROR mengevakuasi cairan lewat pompa limbah dan
      reset ditahan sampai evakuasi selesai.
    - Drain punya fallback waktu (10 ml/detik + margin) bila pompa limbah
      belum dikalibrasi.

  Perubahan besar dari v0.6.6:
    - safety.h: E-stop interrupt + latch, tabel error E01-E10
    - sensor.h: sensor level/suhu/MQ135 aktif di semua halaman (sensorTick)
    - process.h: mesin proses siklus (dosing berurutan, agitasi, drain, bilas)
    - motor.h / dcmotor.h: model kalibrasi a*ml+b dan r*detik+c + titik uji
    - storage.h: preset v2, kalibrasi v4, kal2, set, tank, runstate
    - ui.h: layar proses, WAIT_PCB, konfirmasi siklus, error/E-stop,
            pemulihan, pengaturan, tangki, info/status
  Driver lama (stepper, ramp DC, tombol, buzzer, preset, kalibrasi) tetap.

  Library: Wire, Adafruit_GFX, Adafruit_SSD1306, Preferences,
           FastAccelStepper, OneWire, DallasTemperature.
*/

#include <Wire.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "dbg.h"
#include "config.h"
#include "input.h"
#include "data.h"
#include "presetops.h"
#include "sensor.h"
#include "motor.h"
#include "dcmotor.h"
#include "storage.h"
#include "buzzer.h"
#include "safety.h"
#include "process.h"
#include "ui.h"
#include "api.h"
#include "net.h"
#include "ota.h"

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

// ================= HELPER PENGATURAN =================
void ubahPengaturan(int arah) {
  switch (settingCursor) {
    case 0:
      pengaturan.tempPeringatanX10 =
          constrain(pengaturan.tempPeringatanX10 + arah * 5,
                    200, pengaturan.tempBahayaX10 - 10);
      break;
    case 1:
      pengaturan.tempBahayaX10 =
          constrain(pengaturan.tempBahayaX10 + arah * 5,
                    pengaturan.tempPeringatanX10 + 10, 1200);
      break;
    case 2:
      pengaturan.drainMarginPct =
          constrain(pengaturan.drainMarginPct + arah * 5, 0, 100);
      break;
    case 3:
      pengaturan.levelStabilMs =
          constrain(pengaturan.levelStabilMs + arah * 500, 1000, 60000);
      break;
    case 6:
      pengaturan.kapHclMl = constrain(pengaturan.kapHclMl + arah * 100, 100, 5000);
      break;
    case 7:
      pengaturan.kapH2o2Ml = constrain(pengaturan.kapH2o2Ml + arah * 100, 100, 5000);
      break;
    case 8:
      pengaturan.kapH2oMl = constrain(pengaturan.kapH2oMl + arah * 100, 100, 5000);
      break;
    case 9:
      pengaturan.kapSabunMl = constrain(pengaturan.kapSabunMl + arah * 100, 100, 5000);
      break;
    case 10:
      pengaturan.kapLimbahMl = constrain(pengaturan.kapLimbahMl + arah * 500, 500, 50000);
      break;
    default:
      // Baris MQ135/IoT berupa ON/OFF: diubah lewat ENTER, bukan KIRI/KANAN.
      break;
  }
  clampTankKeKapasitas();
  perluGambar = true;
}

// ================= HELPER TANGKI =================
long* tankPtr(int idx) {
  switch (idx) {
    case 0: return &tankHclMl;
    case 1: return &tankH2o2Ml;
    case 2: return &tankH2oMl;
    case 3: return &tankSabunMl;
    case 4: return &tankLimbahMl;
    default: return nullptr;
  }
}

void ubahTank(int arah) {
  long* t = tankPtr(tankEditIdx);
  if (t == nullptr) return;
  long v = *t + (long)arah * tankEditStep;
  *t = constrain(v, 0L, (long)kapasitasTank(tankEditIdx));
  perluGambar = true;
}

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

// ================= RESET REASON & WATCHDOG =================
bool resetKarenaWdt = false;

void catatAlasanReset() {
  esp_reset_reason_t r = esp_reset_reason();
  const char* s = "?";
  switch (r) {
    case ESP_RST_POWERON:  s = "PWRON";   break;
    case ESP_RST_EXT:      s = "EXT";     break;
    case ESP_RST_SW:       s = "SW";      break;
    case ESP_RST_PANIC:    s = "PANIC";   break;
    case ESP_RST_INT_WDT:  s = "INT_WDT"; resetKarenaWdt = true; break;
    case ESP_RST_TASK_WDT: s = "TASK_WDT";resetKarenaWdt = true; break;
    case ESP_RST_WDT:      s = "WDT";     resetKarenaWdt = true; break;
    case ESP_RST_DEEPSLEEP:s = "DEEP";    break;
    case ESP_RST_BROWNOUT: s = "BROWNOUT";break;
    case ESP_RST_SDIO:     s = "SDIO";    break;
    default:               s = "LAIN";    break;
  }
  infoAlasanReset = s;
  dbg.print(F("[BOOT] Alasan reset: "));
  dbg.println(s);
}

void watchdogInit() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms     = (uint32_t)WDT_TIMEOUT_S * 1000UL;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic  = true;

  esp_err_t r = esp_task_wdt_init(&cfg);
  if (r == ESP_ERR_INVALID_STATE) {
    // Arduino core mungkin sudah menyalakan TWDT saat boot.
    r = esp_task_wdt_reconfigure(&cfg);
  }
  if (r == ESP_OK) {
    esp_task_wdt_add(NULL);
    dbg.print(F("[WDT] Aktif "));
    dbg.print(WDT_TIMEOUT_S);
    dbg.println(F(" s"));
  } else {
    dbg.print(F("[WDT] Gagal init, kode "));
    dbg.println((int)r);
  }
}

// LED strip/indikator GPIO7. Bila modul WS2812, pin data dibiarkan LOW
// (padam) karena library LED tidak dipakai di firmware standalone ini.
void ledStripInit() {
  pinMode(PIN_LED_STRIP, OUTPUT);
  digitalWrite(PIN_LED_STRIP, LOW);
}

// ================= NAVIGASI =================
void tanganiTombol(int t) {
  dbg.print(F("[TOMBOL] "));
  dbg.println(namaTombol[t]);
  buzzerKlik();                 // feedback bunyi saat tombol ditekan
  perluGambar = true;

  // ---- Overlay keselamatan paling prioritas ----
  if (safetyState != SAF_AMAN) {
    if (t == P_ENTER) {
      if (!safetyKonfirmasi()) {
        if (evakuasiAktif) tampilToast("Tunggu drain selesai");
        else tampilToast(safetyState == SAF_ESTOP ? "Lepas E-stop dulu" : "Tidak bisa");
      } else if (halaman == HAL_PROSES && !prosesJalan) {
        pindahHalaman(HAL_PRESET_DETAIL);
      }
    }
    return;
  }

  // ---- Overlay pemulihan setelah mati listrik ----
  if (pemulihanTersedia) {
    if (t == P_ATAS) {
      konfirmasiSel = (konfirmasiSel + 2) % 3;
    } else if (t == P_BAWAH) {
      konfirmasiSel = (konfirmasiSel + 1) % 3;
    } else if (t == P_ENTER) {
      if (konfirmasiSel == 0)      prosesMulaiPemulihan(true);
      else if (konfirmasiSel == 1) prosesBatalPemulihan();
      else                         prosesAbaikanPemulihan();
      if (prosesJalan) {
        pindahHalaman(HAL_PROSES);
      } else if (!pemulihanTersedia) {
        pindahHalaman(HAL_MENU_UTAMA);
      }
    }
    return;
  }

  switch (halaman) {

    case HAL_MENU_UTAMA:
      if (t == P_ATAS)  listGeser(L_UTAMA, -1, JUMLAH_MENU);
      if (t == P_BAWAH) listGeser(L_UTAMA, +1, JUMLAH_MENU);
      if (t == P_ENTER) {
        switch (lsSel[L_UTAMA]) {
          case 0:
            listReset(L_PRESET);
            pindahHalaman(HAL_PRESET_LIST);
            break;
          case 1:
            listReset(L_CFG);
            pindahHalaman(HAL_CFG_LIST);
            break;
          case 2:
            levelMasuk();
            pindahHalaman(HAL_LEVEL);
            break;
          case 3:
            listReset(L_AKSI);
            pindahHalaman(HAL_KALIB_MAIN);
            break;
          case 4:
            settingCursor = 0;
            settingTop = 0;
            pindahHalaman(HAL_PENGATURAN);
            break;
          case 5:
            sinkronTankDariSensor();
            tankEditIdx = 0;
            tankScroll = 0;
            tampilToast("ENTER: penuh/kosong");
            pindahHalaman(HAL_TANK);
            break;
          case 6:
            iotScroll = 0;
            pindahHalaman(HAL_IOT_STATUS);
            break;
          case 7:
            infoScroll = 0;
            pindahHalaman(HAL_INFO);
            break;
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
      int maxScroll = max(0, DETAIL_CONTENT_H - (AREA_BAWAH - HEADER_H));
      if (t == P_ATAS)  presetDetailScroll = max(0, presetDetailScroll - DETAIL_SCROLL_STEP);
      if (t == P_BAWAH) presetDetailScroll = min(maxScroll, presetDetailScroll + DETAIL_SCROLL_STEP);
      if (t == P_ENTER) {
        if (safetyState != SAF_AMAN) {
          tampilToast("E-STOP/ERROR aktif");
          break;
        }
        if (!pompaSiapSemua()) {
          tampilToast("Kalibrasi pompa dulu");
          break;
        }
        if (otaAktif) {
          tampilToast("OTA sedang berjalan");
          break;
        }
        if (prosesJalan) {
          // Proses sedang dijeda: kembali ke layar jeda, jangan tawarkan drain.
          pindahHalaman(HAL_PROSES);
          break;
        }
        if (volumeWadah > 0) {
          // Sisa proses sebelumnya: tawarkan drain sebelum mulai lagi.
          dialogBatal = true;
          pindahHalaman(HAL_PROSES);
          break;
        }
        prosesMulai();
        pindahHalaman(HAL_PROSES);
      }
      if (t == P_BACK)  pindahHalaman(HAL_PRESET_LIST);
      break;
    }

    case HAL_PROSES:
      if (prosesJeda) {
        // Layar jeda: Lanjutkan / Batalkan total / Abaikan (kembali ke menu).
        if (t == P_ATAS) {
          jedaSel = (jedaSel + 2) % 3;
        } else if (t == P_BAWAH) {
          jedaSel = (jedaSel + 1) % 3;
        } else if (t == P_ENTER) {
          if (jedaSel == 0) {
            prosesToggleJeda();
          } else if (jedaSel == 1) {
            prosesBatal();
            if (!prosesJalan) pindahHalaman(HAL_PRESET_DETAIL);
          } else {
            prosesAbaikanJeda();
            pindahHalaman(HAL_MENU_UTAMA);
          }
        } else if (t == P_BACK) {
          pindahHalaman(HAL_MENU_UTAMA);
        }
        break;
      }
      if (dialogBatalMinta) {
        // Konfirmasi "Batalkan proses?"
        if (t == P_ENTER) {
          prosesBatal();
          dialogBatalMinta = false;
          // Bila wadah berisi, prosesBatal langsung menyalakan drain dan
          // tetap di layar proses. Bila kosong, kembali ke detail preset.
          if (!prosesJalan) pindahHalaman(HAL_PRESET_DETAIL);
        } else if (t == P_BACK) {
          dialogBatalMinta = false;
          dbg.println(F("[PROSES] Batal dibatalkan (lanjut)"));
        }
      } else if (dialogBatal) {
        // Tawaran "Drain wadah?" untuk sisa proses sebelumnya.
        if (t == P_ENTER) {
          prosesDrainBatal();
          if (!prosesJalan) pindahHalaman(HAL_PRESET_DETAIL);
        } else if (t == P_BACK) {
          // User menyatakan nampan sudah kosong: hapus ledger agar peringatan
          // tidak muncul lagi saat preset dijalankan.
          dialogBatal = false;
          volumeWadah = 0;
          hapusRunstate();
          if (tankDataAda) simpanTank();
          tampilToast("Nampan dianggap kosong");
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      } else if (prosesSelesai()) {
        if (t == P_ENTER || t == P_BACK) {
          prosesKeluarSelesai();
          pindahHalaman(HAL_PRESET_DETAIL);
        }
      } else if (prosesDrainBatalAktif()) {
        // Drain otomatis setelah cancel: biarkan selesai, BACK tidak
        // memotong proses penyedotan.
        if (t == P_BACK) tampilToast("Drain berjalan");
      } else if (!prosesJalan) {
        // Proses sudah berhenti (mis. drain batal selesai).
        if (t == P_ENTER || t == P_BACK) pindahHalaman(HAL_PRESET_DETAIL);
      } else if (prosesTungguPcb) {
        if (t == P_ENTER) {
          prosesKonfirmasiPcb();
        } else if (t == P_BACK) {
          prosesBatal();
          if (!prosesJalan) pindahHalaman(HAL_PRESET_DETAIL);
        }
      } else if (prosesKonfirmasi) {
        if (t == P_ATAS || t == P_BAWAH) {
          konfirmasiSel = 1 - konfirmasiSel;
        } else if (t == P_ENTER) {
          prosesKonfirmasiSiklus(konfirmasiSel == 0);
        }
      } else {
        if (t == P_ENTER) prosesToggleJeda();
        if (t == P_BACK) {
          dialogBatalMinta = true;
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
        } else if (a == 4) {
          // Diagnostik E-stop (GPIO8)
          pindahHalaman(HAL_KALIB_ESTOP);
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
        if (pompaSudahKalibrasi(kalibMotorIdx)) tampilToast("Kalibrasi tersimpan");
        if (kalibMotorIdx <= 2) {
          // Pompa HCl, H2O2, Air (stepper)
          kalibStepperScroll = 0;
          // Run adalah hitungan sesi: mulai dari 0 setiap kali masuk halaman.
          kalibStepperRunKe[kalibMotorIdx]         = 0;
          kalibStepperLulus[kalibMotorIdx]         = false;
          kalibStepperKoreksiBesar[kalibMotorIdx]  = false;
          kalibStepperPriming[kalibMotorIdx]       = false;
          kalibStepperCekGelas[kalibMotorIdx]      = false;
          kalibStepperPrimeMs[kalibMotorIdx]       = 0;
          kalibStepperUkurMl10[kalibMotorIdx]      = 0;
          kalibStepperOffsetAda[kalibMotorIdx]     = false;
          kalibStepperStepUsed[kalibMotorIdx]      = 0;
          kalibStepperHasilTerakhir[kalibMotorIdx] = 0;
          kalibStepperStepTerakhir[kalibMotorIdx]  = 0;
          kalibStepperUkur = false;
          pindahHalaman(HAL_KALIB_STEPPER);
        } else if (kalibMotorIdx <= 4) {
          // Pompa Sabun, Limbah (DC)
          kalibDCScroll = 0;
          kalibDCCursor = 0;
          kalibDcResetSesi(kalibMotorIdx - 3);
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
    // Alur kalibrasi 100 ml: target tetap 100 ml, priming terpisah, hasil ukur langsung
    // dikoreksi menjadi a = langkah/hasil dan b = 0.
    case HAL_KALIB_STEPPER: {
      int idx = kalibMotorIdx;
      bool jalan = pompaAktif(idx);

      if (t == P_BACK) {
        if (jalan) pompaStop(idx);
        kalibStepperPriming[idx]  = false;
        kalibStepperPrimeMs[idx]  = 0;
        kalibStepperCekGelas[idx] = false;
        kalibStepperUkur          = false;
        kalibStepperStepUsed[idx] = 0;
        kalibStepperScroll = 0;
        pindahHalaman(HAL_KALIB_MOTOR_LIST);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        int arah = (t == P_BAWAH) ? +1 : -1;
        kalibStepperCursor = (kalibStepperCursor + arah + 8) % 8;
        // Auto-scroll: pastikan baris terpilih sepenuhnya terlihat
        {
          int maxScroll = max(0, KALIB_STEPPER_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
          int visibleH  = AREA_BAWAH - (HEADER_H + 4);
          int top = KALIB_STEPPER_ROW_TOP[kalibStepperCursor];
          int h   = KALIB_STEPPER_ROW_H[kalibStepperCursor];
          if (top - kalibStepperScroll + h > visibleH)
            kalibStepperScroll = top + h - visibleH;
          if (top - kalibStepperScroll < 0)
            kalibStepperScroll = top;
          if (kalibStepperScroll > maxScroll) kalibStepperScroll = maxScroll;
          if (kalibStepperScroll < 0) kalibStepperScroll = 0;
        }
        break;
      }
      if (t == P_KIRI || t == P_KANAN) {
        int arah = (t == P_KANAN) ? +1 : -1;
        if (kalibStepperCursor == 2) {
          // RPM hanya boleh diubah saat pompa diam.
          if (jalan) { tampilToast("Stop dulu"); break; }
          pompaAturRpmLive(idx, kalibStepperRpm[idx] + arah * PUMP_RPM_STEP);
        } else if (kalibStepperCursor == 4) {
          // Hasil ukur dalam 0,1 ml.
          if (jalan) { tampilToast("Stop dulu"); break; }
          kalibStepperUkurMl10[idx] = constrain(
              kalibStepperUkurMl10[idx] + arah * kalibStepperStep,
              KALIB_ML_MIN * 10, KALIB_ML_MAX * 10);
          // Hasil berubah -> konfirmasi koreksi besar harus diulang.
          kalibStepperKoreksiBesar[idx] = false;
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibStepperCursor == 0) {
          // START / STOP run kalibrasi 100 ml
          if (jalan) {
            if (kalibStepperPriming[idx]) {
              tampilToast("Stop priming");
              break;
            }
            pompaStop(idx);
            kalibStepperPriming[idx]  = false;
            kalibStepperPrimeMs[idx]  = 0;
            kalibStepperUkur          = false;
            kalibStepperStepUsed[idx] = 0;   // run parsial tidak boleh dikoreksi
            kalibStepperCekGelas[idx] = true;  // gelas berisi sisa run parsial
            tampilToast("Dihentikan");
          } else {
            // Setelah koreksi diterapkan, pastikan gelas sudah dikosongkan dan
            // dikeringkan sebelum run berikutnya. ENTER kedua baru memulai.
            if (kalibStepperCekGelas[idx]) {
              kalibStepperCekGelas[idx] = false;
              tampilToast("Kosongkan dulu");
              break;
            }
            int mulai = pompaKalibrasi100Mulai(idx);
            if (mulai == 1) {
              kalibStepperPriming[idx]  = false;
              kalibStepperPrimeMs[idx]  = 0;
              kalibStepperCekGelas[idx] = false;
              kalibStepperUkur          = true;
              kalibStepperUkurMl10[idx] = 0;
              tampilToast("Ramp ke RPM...");
            } else if (mulai == 2) {
              tampilToast("Cek pompa/prime");
            } else {
              tampilToast("Gagal start");
            }
          }
        } else if (kalibStepperCursor == 1) {
          // Priming: pompa kontinu sampai cairan keluar.
          if (jalan) {
            if (!kalibStepperPriming[idx]) {
              tampilToast("Stop START dulu");
              break;
            }
            pompaStop(idx);
            kalibStepperPriming[idx] = false;
            kalibStepperPrimeMs[idx] = 0;
            kalibStepperUkur         = false;
            tampilToast("Priming stop");
          } else if (pompaTestMulai(idx)) {
            kalibStepperPriming[idx]  = true;
            kalibStepperPrimeMs[idx]  = millis();
            kalibStepperUkur          = false;
            kalibStepperStepUsed[idx] = 0;
            tampilToast("Priming 10 dtk+");
          } else {
            tampilToast("Gagal start");
          }
        } else if (kalibStepperCursor == 2) {
          tampilToast("KIRI/KANAN");
        } else if (kalibStepperCursor == 3) {
          tampilToast("Target tetap");
        } else if (kalibStepperCursor == 4) {
          // ENTER hasil ukur = terapkan koreksi model.
          if (jalan) { tampilToast("Stop dulu"); break; }
          float hasilMl = kalibStepperUkurMl10[idx] / 10.0f;
          if (kalibStepperUkurMl10[idx] <= 0) {
            // Isi cepat: ENTER pada hasil kosong = 100 ml. Tidak langsung
            // diterapkan supaya user masih bisa menyesuaikan.
            kalibStepperUkurMl10[idx] = KALIB_STEPPER_TARGET_ML * 10;
            tampilToast("Isi 100 ml");
          } else if (hasilMl < (float)KALIB_ML_MIN) {
            tampilToast("Isi hasil dulu");
          } else if (kalibStepperStepUsed[idx] <= 0) {
            tampilToast("START dulu");
          } else {
            float offsetPct = (hasilMl - (float)KALIB_STEPPER_TARGET_ML)
                              * 100.0f / (float)KALIB_STEPPER_TARGET_ML;
            bool turunDrastis =
                (kalibStepperHasilTerakhir[idx] > 0) &&
                ((float)kalibStepperUkurMl10[idx] <
                 (float)kalibStepperHasilTerakhir[idx] *
                     KALIB_STEPPER_TURUN_PCT / 100.0f) &&
                (kalibStepperStepUsed[idx] >= kalibStepperStepTerakhir[idx]);

            if (turunDrastis && !kalibStepperKoreksiBesar[idx]) {
              kalibStepperKoreksiBesar[idx] = true;
              tampilToast("Cek priming!");
            } else if (fabsf(offsetPct) > KALIB_STEPPER_KOREKSI_BESAR
                       && !kalibStepperKoreksiBesar[idx]) {
              kalibStepperKoreksiBesar[idx] = true;
              tampilToast(">50%, ENTER lagi");
            } else if (pompaKalibrasiTerapkan(idx, hasilMl)) {
              simpanPompa();
              kalibTandaiWaktu(idx);
              kalibStepperUkurMl10[idx] = 0;
              kalibStepperStepUsed[idx] = 0;
              kalibStepperCekGelas[idx] = true;   // run berikutnya wajib gelas kosong
              kalibStepperCursor        = 0;
              kalibStepperScroll        = 0;
              if (kalibStepperRunKe[idx] >= KALIB_STEPPER_RUN_MIN)
                tampilToast(kalibStepperLulus[idx] ? "3x LULUS" : "3x ULANGI");
              else
                tampilToast(kalibStepperLulus[idx] ? "Bagus, ulangi" : "Dikoreksi, ulang");
            } else {
              tampilToast("Hasil tak wajar");
            }
          }
        } else if (kalibStepperCursor == 5) {
          // ENTER di baris Koreksi mengganti resolusi input Hasil.
          if      (kalibStepperStep == 1)  { kalibStepperStep = 10; tampilToast("Step 1 ml"); }
          else if (kalibStepperStep == 10) { kalibStepperStep = 50; tampilToast("Step 5 ml"); }
          else                             { kalibStepperStep = 1;  tampilToast("Step 0.1 ml"); }
        } else if (kalibStepperCursor == 7) {
          // Reset model -> kembali ke base nominal.
          if (jalan) { tampilToast("Stop dulu"); break; }
          pompaResetTitik(idx);
          kalibStepperRunKe[idx]          = 0;
          kalibStepperLulus[idx]          = false;
          kalibStepperKoreksiBesar[idx]   = false;
          kalibStepperPriming[idx]        = false;
          kalibStepperPrimeMs[idx]        = 0;
          kalibStepperCekGelas[idx]       = false;
          kalibStepperUkurMl10[idx]       = 0;
          kalibStepperOffsetX10[idx]      = 0;
          kalibStepperOffsetAda[idx]      = false;
          kalibStepperStepUsed[idx]       = 0;
          kalibStepperHasilTerakhir[idx]  = 0;
          kalibStepperStepTerakhir[idx]   = 0;
          simpanPompa();
          kalibTandaiWaktu(idx);
          tampilToast("Direset ke base");
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
          int baru = constrain(kalibDutyDC[idx] + arah * 5, 0, 100);
          if (baru != kalibDutyDC[idx]) {
            kalibDutyDC[idx] = baru;
            // Laju pompa bergantung duty: model lama tidak lagi berlaku.
            kalibDcResetModel(idx);
            kalibDcResetSesi(idx);
            int ch = idx + 1;
            if (dcAktif(ch)) dcSetPct(ch, kalibDutyDC[idx]);   // live, ikut ramp
            tampilToast("Duty berubah, kalibrasi");
          }
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
    // Run 1 sesi = pulse pendek tanpa model; run 2+ = target ml.
    // Model diperbarui dari hasil ukur terakhir (r = ml/detik, c = 0)
    // sehingga satu koreksi langsung mendekati laju nyata.
    case HAL_KALIB_DC: {
      // v1.0.2: run tetap per pompa (sabun 100 s, limbah 20 s), user mengisi hasil ml,
      // laju = ml / detik, diulang KALIB_DC_RUN_MIN kali lalu dirata-rata.
      // Baris: 0 START/STOP, 1 Duty, 2 Hasil (ENTER = terapkan), 3 Step,
      //        4 Laju run, 5 1 ml = ms, 6 Run, 7 Reset
      int idx = kalibMotorIdx - 3;                 // 0=sabun, 1=limbah
      int ch  = idx + 1;
      bool ujiJalan = kalibDcJalan() || dcAktif(ch) || dcTargetPct[ch] > 0.0f;

      if (t == P_BACK) {
        if (kalibDcJalan()) kalibDcStop();
        if (dcAktif(ch) || dcTargetPct[ch] > 0.0f) dcStop(ch);
        kalibDcUkur = false;
        kalibDcDurasiMs = 0;
        simpanPompa();
        kalibDCScroll = 0;
        pindahHalaman(HAL_KALIB_MOTOR_LIST);
        break;
      }
      if (t == P_ATAS || t == P_BAWAH) {
        int arah = (t == P_BAWAH) ? +1 : -1;
        kalibDCCursor = (kalibDCCursor + arah + 8) % 8;
        {
          int maxScroll = max(0, KALIB_DC_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
          int visibleH  = AREA_BAWAH - (HEADER_H + 4);
          int top = KALIB_DC_ROW_TOP[kalibDCCursor];
          int h   = KALIB_DC_ROW_H[kalibDCCursor];
          if (top - kalibDCScroll + h > visibleH)
            kalibDCScroll = top + h - visibleH;
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
          if (ujiJalan) { tampilToast("Stop dulu"); break; }
          int baru = constrain(kalibDutyDC[idx] + arah * 5, 0, 100);
          if (baru != kalibDutyDC[idx]) {
            kalibDutyDC[idx] = baru;
            // Laju bergantung pada duty: model lama tidak lagi berlaku.
            kalibDcResetModel(idx);
            kalibDcResetSesi(idx);
            tampilToast("Duty berubah, kalibrasi");
          }
        } else if (kalibDCCursor == 2) {
          if (ujiJalan) { tampilToast("Stop dulu"); break; }
          kalibDcUkurMl[idx] = constrain(
              kalibDcUkurMl[idx] + arah * kalibDcStep, 0, KALIB_DC_ML_MAX);
        } else if (kalibDCCursor == 3) {
          // Step edit hasil: 1 -> 10 -> 100 ml
          if      (arah > 0) kalibDcStep = (kalibDcStep == 1) ? 10 : (kalibDcStep == 10) ? 100 : 1;
          else               kalibDcStep = (kalibDcStep == 100) ? 10 : (kalibDcStep == 10) ? 1 : 100;
        }
        break;
      }
      if (t == P_ENTER) {
        if (kalibDCCursor == 0) {
          if (ujiJalan) {
            kalibDcStop();               // ramp turun, durasi perintah dicatat
            dcStop(ch);
            kalibDcUkur = false;
            kalibDcDurasiMs = 0;
            tampilToast("Dihentikan");
          } else if (kalibDutyDC[idx] <= 0) {
            tampilToast("Duty 0%");
          } else {
            kalibDcPulseRun[idx] = false;
            kalibDcUkurMl[idx]   = 0;
            kalibDcUjiAda[idx]   = false;
            kalibDcDurasiMs      = kalibDcDurasiRun(idx);
            kalibDcUkur          = true;
            kalibDcMulai(idx);
            if (kalibDcTitikN[idx] >= KALIB_DC_RUN_MIN) tampilToast("Set kalibrasi baru");
            else {
              static char tb[20];   // toast hanya menyimpan pointer
              snprintf(tb, sizeof(tb), "Run %lu detik", kalibDcDurasiRun(idx) / 1000UL);
              tampilToast(tb);
            }
          }
        } else if (kalibDCCursor == 2) {
          // ---- Terapkan hasil ukur ke model (rata-rata run)
          if (ujiJalan) { tampilToast("Stop dulu"); break; }
          float hasilMl = (float)kalibDcUkurMl[idx];
          float detik   = kalibDcDetikAktual;
          if (hasilMl <= 0.0f) {
            tampilToast("Isi hasil dulu");
          } else if (detik <= 0.0f) {
            tampilToast("Jalankan START dulu");
          } else if (!kalibDcTambahRun(idx, detik, hasilMl)) {
            tampilToast("Laju tidak wajar");
          } else {
            kalibDcUjiAda[idx] = true;
            kalibDcUkurMl[idx] = 0;
            kalibDcDurasiMs    = 0;
            kalibDcDetikAktual = 0.0f;
            simpanPompa();
            kalibTandaiWaktu(3 + idx);
            if (kalibDcTitikN[idx] >= KALIB_DC_RUN_MIN) tampilToast("3x selesai, rata2");
            else tampilToast("Tersimpan, run lagi");

            dbg.print(F("[KALIB-DC] idx="));
            dbg.print(idx);
            dbg.print(F(" hasil="));
            dbg.print(hasilMl);
            dbg.print(F(" detik="));
            dbg.print(detik);
            dbg.print(F(" run="));
            dbg.print(kalibDcTitikN[idx]);
            dbg.print(F(" r_rata2="));
            dbg.print(kalibDcMlPerSec[idx]);
            dbg.print(F(" ml/s, 1ml="));
            dbg.print(1000.0f / kalibDcMlPerSec[idx]);
            dbg.println(F(" ms"));
          }
        } else if (kalibDCCursor == 3) {
          kalibDcStep = (kalibDcStep == 1) ? 10 : (kalibDcStep == 10) ? 100 : 1;
        } else if (kalibDCCursor == 7) {
          // ---- Reset model + sesi kalibrasi
          if (ujiJalan) { tampilToast("Stop dulu"); break; }
          kalibDcResetModel(idx);
          kalibDcResetSesi(idx);
          simpanPompa();
          kalibTandaiWaktu(3 + idx);
          tampilToast("Kalibrasi direset");
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
        kalibTandaiWaktu(5);
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
      if (t == P_BACK) {
        simpanKal2();
        kalibSuhuScroll = 0;
        pindahHalaman(HAL_KALIB_MAIN);
        break;
      }
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
      if (t == P_BACK) {
        simpanKal2();
        pindahHalaman(HAL_KALIB_MAIN);
        break;
      }
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
        simpanKal2();
        dbg.print(F("[KALIB-MQ135] "));
        dbg.print(kalibUdaraSel == 0 ? F("normal") : F("kotor"));
        dbg.print(F(" adc="));
        dbg.print(mq135Bacaan);
        dbg.print(F(" ppm="));
        dbg.println(ppm);
      }
      break;

    // ---------- Diagnostik E-stop (GPIO8) ----------
    case HAL_KALIB_ESTOP:
      if (t == P_BACK) pindahHalaman(HAL_KALIB_MAIN);
      break;

    // ---------- Kalibrasi Sensor Level: Pilih Cairan ----------
    case HAL_KALIB_LEVEL: {
      const int ROW_H = 10;
      const int VISIBLE = (AREA_BAWAH - HEADER_H) / ROW_H;
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
        // v1.0.3: 0% DAN 100% sama-sama langsung disimpan ke NVS bila pasangan
        // nilainya layak (100% > 0%). Sebelumnya 0% hanya masuk RAM.
        if (kalibSetStep == 0) sensorSetKosong(kalibSensorIdx);
        else                   sensorSetPenuh(kalibSensorIdx);
        dbg.print(kalibSetStep == 0 ? F("[KALIB] 0% = ") : F("[KALIB] 100% = "));
        dbg.println(kalibSetStep == 0 ? sensorKalibKosong[kalibSensorIdx]
                                      : sensorKalibPenuh[kalibSensorIdx]);
        if (!sensorKalibLayak(kalibSensorIdx)) {
          tampilToast("100% harus > 0%");
        } else if (simpanKalib()) {
          tampilToast(kalibSetStep == 0 ? "0% tersimpan OK" : "100% tersimpan OK");
          if (kalibSetStep == 1) pindahHalaman(HAL_KALIB_LEVEL);
        } else {
          tampilToast("Simpan GAGAL");
        }
      }
      break;

    // ---------- Pengaturan global ----------
    case HAL_PENGATURAN:
      if (t == P_BACK) {
        simpanSet();
        pindahHalaman(HAL_MENU_UTAMA);
      } else if (t == P_ATAS) {
        settingCursor = max(0, settingCursor - 1);
      } else if (t == P_BAWAH) {
        settingCursor = min(JUMLAH_SET - 1, settingCursor + 1);
      } else if (t == P_KIRI || t == P_KANAN) {
        ubahPengaturan((t == P_KANAN) ? +1 : -1);
      } else if (t == P_ENTER) {
        if (settingCursor == 4) {
          pengaturan.mqAktif = pengaturan.mqAktif ? 0 : 1;
          tampilToast(pengaturan.mqAktif ? "MQ135 ON" : "MQ135 OFF");
        } else if (settingCursor == 5) {
          pengaturan.iotEnabled = pengaturan.iotEnabled ? 0 : 1;
          tampilToast(pengaturan.iotEnabled ? "IoT ON" : "IoT OFF");
        } else {
          simpanSet();
          tampilToast("Tersimpan");
        }
      }
      break;

    // ---------- Isi Ulang Tangki ----------
    case HAL_TANK_EDIT:
      break;
    case HAL_TANK:
      if (t == P_BACK) {
        simpanTank();
        pindahHalaman(HAL_MENU_UTAMA);
      } else if (t == P_ATAS) {
        tankEditIdx = max(0, tankEditIdx - 1);
      } else if (t == P_BAWAH) {
        tankEditIdx = min(4, tankEditIdx + 1);
      } else if (t == P_KIRI || t == P_KANAN) {
        ubahTank((t == P_KANAN) ? +1 : -1);
      } else if (t == P_ENTER) {
        long* tp = tankPtr(tankEditIdx);
        if (tp != nullptr) {
          if (tankEditIdx == 4) {
            *tp = 0;                       // limbah: kosongkan
            tampilToast("Limbah dikosongkan");
          } else {
            *tp = kapasitasTank(tankEditIdx);  // sumber: isi penuh
            tampilToast("Tangki penuh");
          }
        }
        simpanTank();
        perluGambar = true;
      }
      break;

    // ---------- Status IoT ----------
    case HAL_IOT_STATUS:
      if (t == P_ATAS) iotScroll = max(0, iotScroll - 1);
      if (t == P_BAWAH) iotScroll++;
      if (t == P_BACK) {
        iotScroll = 0;
        pindahHalaman(HAL_MENU_UTAMA);
      }
      break;

    // ---------- Info / Status ----------
    case HAL_INFO: {
      const int contentH = 11 * 9 + 2;
      int maxScroll = max(0, contentH - (AREA_BAWAH - HEADER_H));
      if (t == P_ATAS)   infoScroll = max(0, infoScroll - 9);
      if (t == P_BAWAH)  infoScroll = min(maxScroll, infoScroll + 9);
      if (t == P_BACK) {
        infoScroll = 0;
        pindahHalaman(HAL_MENU_UTAMA);
      }
      break;
    }

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

#ifdef DEBUG_BOOT_WAIT
  // Hanya untuk debugging USB: tunggu host lalu cetak ulang banner.
  delay(2000);
  dbgInit();
#endif

  dbg.print(F("[BOOT] Menu_OLED_6Button "));
  dbg.println(F(FW_VERSI));
  apiInit();
  netInit();

  catatAlasanReset();

  // Muat data NVS. muatKalib() dipanggil SETELAH sensorInit() karena
  // sensorInit() mengisi nilai default kalibrasi.
  muatPreset();
  muatPompa();
  muatKalibTs();
  muatKal2();
  muatSet();
  muatTank();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK);

  if (!tulisPCF8574(0xFF))
    dbg.println(F("!! PCF8574 tidak terdeteksi. Cek wiring/alamat I2C."));
  inputInit();

  if (!sensorInit())
    dbg.println(F("!! Sensor level tidak siap. Cek wiring GPIO."));
  muatKalib();
  sensorSuhuInit();                      // scan address DS18B20 di GPIO10

  motorInit();
  dcmotorInit();
  buzzerInit();
  safetyInit();

  ledStripInit();
  pinMode(PIN_MQ135, INPUT);             // sensor udara MQ-135

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    dbg.println(F("!! OLED tidak terdeteksi di 0x3C. Cek wiring/alamat I2C."));
  } else {
    oled.setRotation(0);
    gambarLayar();
    perluGambar = false;
  }

  // Pemulihan setelah mati listrik: tawarkan drain/abaikan, tidak ada aksi
  // otomatis. Runstate hanya valid bila proses belum selesai.
  int rf = 0, rl = 0, rs = 0, rv = 0, rp = 0;
  if (muatRunstate(rf, rl, rs, rv, rp) && rp >= 0 && rp < jumlahPreset) {
    presetTerpilih = rp;
    konfirmasiSel = 0;
    prosesSetPemulihan(rf, rl, rs, rv, rp);
    dbg.print(F("[BOOT] Runstate ditemukan: fase="));
    dbg.print(rf);
    dbg.print(F(" siklus="));
    dbg.print(rs);
    dbg.print(F(" volume="));
    dbg.println(rv);
  } else {
    hapusRunstate();
  }

  watchdogInit();

  // Nada boot diputar di akhir setup(): kalau dipanggil lebih awal, melodi
  // akan tertahan oleh delay() di atas (buzzerUpdate baru jalan di loop).
  buzzerBoot();

  // E10: beri tahu pengguna bila boot sebelumnya berakhir karena watchdog.
  if (resetKarenaWdt)
    safetyRaiseKonteks(10, "Reset oleh watchdog", "Cek beban loop/log");
}

// ================= LOOP =================
void loop() {
  // Keselamatan lebih dulu, lalu sensor, lalu mesin proses.
  safetyTick();
  sensorTick();

  int t = bacaTombolBaru();
  if (t != TIDAK_ADA) tanganiTombol(t);

  apiUpdate();                               // command Serial non-blocking
  netUpdate();                               // WiFi/MQTT background task
  otaUpdate();                               // OTA hanya saat idle/aman

  // Tahan BACK >= BACK_TAHAN_MS -> kembali ke menu utama. Tidak berlaku saat
  // proses berjalan, layar keselamatan, atau dialog pemulihan.
  static bool backTahanSudah = false;
  if (!stateStabil[P_BACK]) {
    backTahanSudah = false;
  } else if (!backTahanSudah && tombolDitahan(P_BACK, BACK_TAHAN_MS)) {
    backTahanSudah = true;
    if (safetyState == SAF_AMAN && !prosesJalan && !pemulihanTersedia &&
        halaman != HAL_PROSES && halaman != HAL_MENU_UTAMA) {
      pindahHalaman(HAL_MENU_UTAMA);
    }
  }

  prosesUpdate();                            // logika proses, tidak tergantung render
  evakuasiUpdate();                          // pompa limbah saat E-stop/ERROR
  motorUpdate();                             // stepper non-blocking
  dcmotorUpdate();                           // ramp duty DC/agitasi non-blocking
  kalibDcUpdate();                           // state machine uji DC
  buzzerUpdate();                            // pola nada non-blocking

  // Data suhu baru -> redraw halaman yang menampilkannya.
  if (sensorSuhuSiapTampil()) perluGambar = true;

  // Redraw layar proses berkala (progres, suhu, sisa waktu).
  if (halaman == HAL_PROSES && prosesJalan && !prosesJeda) {
    static unsigned long prosesUiMs = 0;
    unsigned long now = millis();
    // 250 ms: progres ml saat dosing terlihat halus.
    if (now - prosesUiMs >= 250) { prosesUiMs = now; perluGambar = true; }
  }

  // Hitung mundur evakuasi darurat (layar safety).
  if (evakuasiAktif) {
    static unsigned long evakUiMs = 0;
    unsigned long now = millis();
    if (now - evakUiMs >= 500) { evakUiMs = now; perluGambar = true; }
  }

  // Redraw halaman kalibrasi stepper saat status START/STOP berubah.
  // Saat run 100 ml selesai sendiri, minta user mengukur hasil nyata.
  if (halaman == HAL_KALIB_STEPPER) {
    static bool stepperJalanTerakhir = false;
    bool jalan = pompaAktif(kalibMotorIdx);
    if (jalan != stepperJalanTerakhir) {
      stepperJalanTerakhir = jalan;
      perluGambar = true;
      if (!jalan) {
        if (kalibStepperUkur) {
          kalibStepperUkur = false;
          // Hasil dikosongkan: user wajib memasukkan angka ukur. ENTER pada
          // hasil kosong akan mengisi cepat 100 ml (belum diterapkan).
          kalibStepperUkurMl10[kalibMotorIdx]     = 0;
          kalibStepperKoreksiBesar[kalibMotorIdx] = false;
          kalibStepperCursor = 4;   // fokus ke baris Hasil
          int visibleH = AREA_BAWAH - (HEADER_H + 4);
          int maxScroll = max(0, KALIB_STEPPER_CONTENT_H - visibleH);
          kalibStepperScroll = KALIB_STEPPER_ROW_TOP[4] + KALIB_STEPPER_ROW_H[4] - visibleH;
          if (kalibStepperScroll < 0) kalibStepperScroll = 0;
          if (kalibStepperScroll > maxScroll) kalibStepperScroll = maxScroll;
          tampilToast("Selesai, ukur ml");
        } else if (kalibStepperPriming[kalibMotorIdx]) {
          kalibStepperPriming[kalibMotorIdx] = false;
          kalibStepperPrimeMs[kalibMotorIdx] = 0;
        }
      }
    }
    // Progres % berubah selama pompa berjalan.
    static unsigned long stepperMs = 0;
    if (jalan) {
      unsigned long now = millis();
      if (now - stepperMs >= 500) { stepperMs = now; perluGambar = true; }
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
        kalibDCCursor = 2;   // fokus ke baris Hasil
        {
          int visibleH  = AREA_BAWAH - (HEADER_H + 4);
          int maxScroll = max(0, KALIB_DC_CONTENT_H - visibleH);
          kalibDCScroll = KALIB_DC_ROW_TOP[2] + KALIB_DC_ROW_H[2] - visibleH;
          if (kalibDCScroll > maxScroll) kalibDCScroll = maxScroll;
          if (kalibDCScroll < 0) kalibDCScroll = 0;
        }
        tampilToast("Selesai, ukur ml");
      }
    }
  }

  // Redraw halaman setting agitasi saat status relay berubah.
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

    // Angka RPM aktual berubah selama ramp, jadi redraw berkala saat stepper
    // berjalan agar user melihat kecepatan berhenti di target.
    static unsigned long testRpmMs = 0;
    if (pompaAktif(0) || pompaAktif(1) || pompaAktif(2)) {
      unsigned long now = millis();
      if (now - testRpmMs >= 250) { testRpmMs = now; perluGambar = true; }
    }
  }

  // Status IoT real-time (WiFi/MQTT/NTP/OTA).
  if (halaman == HAL_IOT_STATUS) {
    static unsigned long iotMs = 0;
    unsigned long now = millis();
    if (now - iotMs >= 500) { iotMs = now; perluGambar = true; }
  }

  // Status E-stop real-time untuk halaman diagnostik.
  if (halaman == HAL_KALIB_ESTOP) {
    static unsigned long estopMs = 0;
    unsigned long now = millis();
    if (now - estopMs >= 250) { estopMs = now; perluGambar = true; }
  }

  // Bacaan real-time untuk halaman kalibrasi MQ-135.
  if (halaman == HAL_KALIB_UDARA) {
    static unsigned long udaraMs = 0;
    unsigned long now = millis();
    if (now - udaraMs >= INTERVAL_UDARA_MS) {
      udaraMs = now;
      perluGambar = true;
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

  esp_task_wdt_reset();
  delay(2);
}
