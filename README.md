# Auto Etching Firmware

Firmware mesin etching PCB otomatis berbasis **ESP32-S3 N16R8**, dengan
layar OLED 128x64, 6 tombol melalui PCF8574, tiga pompa peristaltik
DRV8825, tiga motor DC TB6612FNG, lima sensor level non-contact, tiga
sensor suhu DS18B20, sensor udara MQ-135, dan E-stop.

Versi aktif: **v1.3.1**

Build terakhir: **1.246.654 byte flash (63% dari 1.966.080)**, RAM global
**78.260 byte (23%)**.

## Fitur

- **Preset v3 (maks. 10)** - 19 field, rasio kimia dalam persen atau ml,
  waktu etching otomatis dari tebal tembaga dan laju etch, siklus serta
  bilas per siklus.
- **Proses berulang** - dosing HCl/H2O2/H2O, agitasi, drain, bilas sabun,
  bilas air, lalu siklus berikutnya.
- **PRECHECK menyeluruh** - kebutuhan seluruh proses dihitung per jerigen
  dan kapasitas limbah sebelum aktuator menyala.
- **Drain berbasis volume** - ledger volume nampan, residu, purge, dan
  laju drain minimum; cancel/E-stop tidak mengulang volume penuh.
- **Pause / cancel / recovery** - opsi lanjut, batalkan + drain, atau
  hentikan tanpa drain; `runstate` tersimpan di NVS.
- **Keselamatan** - E-stop latch, tabel error E01-E10, watchdog 15 detik,
  evakuasi cairan, dan pesan fault yang menyebut item serta angkanya.
- **Kalibrasi lengkap** - stepper, pompa DC, agitasi, level, suhu, MQ-135,
  dan diagnostik E-stop.
- **IoT** - WiFi, MQTT TLS 8883, NTP, LWT, remote command/config, event log,
  dan halaman **Status IoT** di OLED.
- **Dashboard self-hosted** - konfigurasi/pilih preset, konfirmasi PCB,
  clear E-stop terkondisi, hasil kalibrasi, panel kiri collapsible, dan OTA
  upload `.bin` dari Settings.
- **OTA HTTP** - `POST /update` port 3232, hanya saat IDLE dan safety AMAN;
  file dapat di-drop/dipilih dari dashboard (server meneruskan ke alat).
- **Non-blocking** - proses, sensor, motor, buzzer, dan UI berbasis `millis()`.

## Menu Utama

| Menu | Fungsi |
|---|---|
| Pilih Preset | Pilih, mulai, jeda, lanjut, batal |
| Konfigurasi Preset | Tambah, duplikat, hapus, edit 19 field |
| Level Cairan | Status dan kalibrasi 0%/100% lima sensor |
| Kalibrasi | Motor, sensor, suhu, udara, E-stop |
| Pengaturan | Ambang suhu, margin drain, kapasitas tangki, IoT |
| Isi Ulang Tangki | Isi volume awal sumber dan limbah |
| Status IoT | WiFi, IP/RSSI, MQTT, broker/TLS, NTP, OTA, heap |
| Info/Status | Versi, suhu, level, MQ-135, alasan reset |

Tombol: **ATAS/BAWAH** navigasi, **KIRI/KANAN** ubah nilai,
**ENTER** pilih/konfirmasi, **BACK** kembali (tahan 1 detik ke menu utama).

## Hardware

| Fungsi | GPIO | Catatan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | OLED 0x3C + PCF8574 0x20 |
| Sensor level HCl / H2O2 / H2O / sabun / limbah | 11 / 12 / 13 / 14 / 3 | `touchRead` non-contact |
| Stepper HCl SLP/STEP/DIR | 1 / 2 / 42 | DRV8825 1/16, 3200 langkah/putaran |
| Stepper H2O2 SLP/STEP/DIR | 41 / 40 / 39 | DRV8825 1/16 |
| Stepper H2O SLP/STEP/DIR | 38 / 47 / 21 | DRV8825 1/16 |
| TB6612FNG PWA/PWB driver 1 | 15 / 16 | agitasi / pompa sabun |
| TB6612FNG PWA driver 2 | 17 | pompa limbah |
| Buzzer | 6 | pasif, LEDC |
| E-stop | 8 | `INPUT_PULLUP`, LOW = stop |
| MQ-135 | 9 | analog |
| DS18B20 | 10 | 1-Wire, 3 sensor |
| Exhaust fan | - | langsung 12 V, selalu menyala |

## Struktur Project

```text
auto-etching-firmware/
├── auto-etching-firmware.ino   # setup/loop, menu, handler tombol
├── src/
│   ├── config.h                # pin, konstanta, FW_VERSI
│   ├── data.h                  # struct, enum, state global
│   ├── dbg.h                   # log Serial
│   ├── input.h                 # PCF8574 + debounce
│   ├── presetops.h             # CRUD preset, model waktu, validasi
│   ├── sensor.h                # level, suhu, MQ-135
│   ├── storage.h               # NVS, checksum, migrasi
│   ├── motor.h                 # stepper FastAccelStepper
│   ├── dcmotor.h               # TB6612FNG, kalibrasi DC
│   ├── buzzer.h                # tone non-blocking
│   ├── safety.h                # E-stop, E01-E10, safety context
│   ├── process.h               # state machine proses
│   ├── ui.h                    # render OLED semua halaman
│   ├── api.h                   # command API, snapshot, event log
│   ├── net.h                   # WiFi/MQTT/NTP/topik/command queue
│   ├── net_ca.h                # CA ISRG Root X2
│   ├── ota.h                   # OTA HTTP
│   └── fontmini.h              # font OLED
├── partitions.csv              # layout 4 MB min_spiffs (dual app)
├── secrets.example.h           # template kredensial (placeholder)
├── secrets.h                   # kredensial lokal, di-ignore Git
├── webapp/                     # dashboard web self-hosted
│   ├── server/                 # Fastify + MQTT bridge + auth/audit
│   ├── web/                    # React/Vite dashboard
│   ├── deploy/                 # systemd + reverse proxy contoh
│   └── README.md
├── README.md
├── DOKUMENTASI.md
├── DEPLOYMENT.md
├── IOT-API.md
├── PANDUAN-TEST-IOT.md
├── LICENSE
├── LOG.md
├── RELEASES.md
└── GUIDE.md
```

## Dokumentasi

| Dokumen | Isi |
|---|---|
| [DOKUMENTASI.md](./DOKUMENTASI.md) | Manual pemakaian, pin map, preset, kalibrasi, error, NVS, checklist uji |
| [IOT-API.md](./IOT-API.md) | Command Serial/MQTT, topik, payload, remote config, event, OTA |
| [PANDUAN-TEST-IOT.md](./PANDUAN-TEST-IOT.md) | Langkah uji WiFi, NTP, TLS, MQTT, event, OTA, hardware |
| [LOG.md](./LOG.md) | Changelog rilis |
| [RELEASES.md](./RELEASES.md) | Registry versi, rollback, prosedur tag rilis |
| [GUIDE.md](./GUIDE.md) | Panduan pemilik repo dan contributor |
| [webapp/README.md](./webapp/README.md) | Setup, keamanan, dan deployment dashboard self-hosted |
| [webapp/ARSITEKTUR-DAN-PROSES.md](./webapp/ARSITEKTUR-DAN-PROSES.md) | Arsitektur web, alur data, deployment 24/7, dan jalur belajar web development |
| [DEPLOYMENT.md](./DEPLOYMENT.md) | File lokal yang tidak di-push dan daftar kredensial yang harus diisi saat deploy |
| [LICENSE](./LICENSE) | MIT License |

## Build dan Flash

Prasyarat library Arduino:

- Adafruit GFX Library dan Adafruit SSD1306
- OneWire dan DallasTemperature
- FastAccelStepper
- ESP32MQTTClient
- ArduinoJson

Compile:

```bash
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  --libraries /home/raihanP/Arduino/libraries \
  --output-dir build_ota .
```

Upload USB pertama (tabel partisi berubah):

```bash
~/DevBox/bin/arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  .
```

Board: **ESP32S3 Dev Module**, upload 921600 (turunkan ke 115200 bila gagal).
Layout partisi: **min_spiffs 4 MB**, dual app ±1,9 MB. File
`partitions.csv` di folder sketch dipakai otomatis. Flash 4 MB aman; board
N16R8 16 MB tetap berjalan dengan layout ini.

OTA setelah firmware berjalan:

```bash
curl -X POST \
  --data-binary @build_ota/auto-etching-firmware.ino.bin \
  -H "X-OTA-Password: <OTA_PASSWORD>" \
  http://<IP_ALAT>:3232/update
```

## Catatan Keamanan

- Kredensial WiFi, broker, dan password OTA ada di `secrets.h` lokal yang
  di-ignore Git. Jangan commit file itu; clone baru menyalin
  `secrets.example.h` lalu mengisi nilainya.
- Dashboard web menyimpan kredensial broker di `webapp/server/.env` lokal
  (di-ignore Git); browser tidak pernah menerimanya.
- Kredensial tetap terbaca dari firmware/NVS di perangkat; gunakan repo
  private, rotasi kredensial bila pernah bocor, dan flash encryption bila
  perlindungan fisik diperlukan.
- E-stop fisik tetap wajib; `clear` E-stop dari dashboard hanya bisa setelah
  tombol fisik dilepas dan evakuasi selesai. Error non-E-stop tetap lokal.
- Uji dengan air sebelum memakai HCl/H2O2. Gunakan APD dan ventilasi.
- Data preset, kalibrasi, tangki, dan `runstate` tersimpan di NVS dan tidak
  hilang saat flash ulang.

## Lisensi

[MIT License](./LICENSE) — Copyright (c) 2026 PCB Express.
