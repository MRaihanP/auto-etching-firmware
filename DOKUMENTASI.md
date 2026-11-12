# Dokumentasi Firmware v1.3.1

Manual ini menjelaskan hardware, build, menu, preset, alur proses, kalibrasi,
error, penyimpanan NVS, IoT, dan checklist pengujian. Untuk protokol
Serial/MQTT lihat [`IOT-API.md`](./IOT-API.md); untuk langkah uji jaringan
lihat [`PANDUAN-TEST-IOT.md`](./PANDUAN-TEST-IOT.md); riwayat versi ada di
[`LOG.md`](./LOG.md) dan [`RELEASES.md`](./RELEASES.md).

## 1. Hardware dan Pin Map

Board: **ESP32-S3 N16R8**, OLED SSD1306 128x64, PCF8574 untuk 6 tombol,
3 driver stepper DRV8825 (1/16), 2 modul TB6612FNG, 5 sensor level
non-contact, 3 DS18B20, MQ-135, buzzer, dan E-stop.

| Fungsi | GPIO | Keterangan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | 400 kHz; OLED 0x3C + PCF8574 0x20 |
| Tombol | PCF8574 | ATAS, BAWAH, KIRI, KANAN, ENTER, BACK |
| Sensor level HCl | 11 | `touchRead` strip nikel |
| Sensor level H2O2 | 12 | `touchRead` |
| Sensor level H2O | 13 | `touchRead` |
| Sensor level sabun | 14 | `touchRead` |
| Sensor level limbah | 3 | `touchRead` |
| Stepper HCl SLP/STEP/DIR | 1 / 2 / 42 | DRV8825 1/16, 3200 langkah/putaran |
| Stepper H2O2 SLP/STEP/DIR | 41 / 40 / 39 | DRV8825 1/16 |
| Stepper H2O SLP/STEP/DIR | 38 / 47 / 21 | DRV8825 1/16 |
| TB6612FNG PWA driver 1 | 15 | motor agitasi |
| TB6612FNG PWB driver 1 | 16 | pompa sabun |
| TB6612FNG PWA driver 2 | 17 | pompa limbah |
| Buzzer pasif | 6 | LEDC |
| E-stop | 8 | `INPUT_PULLUP`, LOW = stop |
| MQ-135 | 9 | analog + divider |
| DS18B20 | 10 | 1-Wire, 3 sensor |
| Exhaust fan | - | langsung 12 V, selalu menyala |

Catatan panel: firmware memakai `UI_Y_OFFSET = 15` karena baris atas OLED
rusak. Jika panel normal, ubah ke `0` di `src/config.h`.

## 2. Build dan Flash

Library Arduino:

- Adafruit GFX Library, Adafruit SSD1306
- OneWire, DallasTemperature
- FastAccelStepper
- ESP32MQTTClient, ArduinoJson

Kredensial WiFi/MQTT/OTA ada di `secrets.h` lokal (di-ignore Git). Clone
baru: salin `secrets.example.h` menjadi `secrets.h`, lalu isi nilainya.
File `secrets.h` tidak boleh di-commit.

Compile:

```bash
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  --libraries /home/raihanP/Arduino/libraries \
  --output-dir build_ota .
```

Upload USB pertama:

```bash
~/DevBox/bin/arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  .
```

- Board: **ESP32S3 Dev Module**. Upload 921600; turunkan ke 115200 bila gagal.
- Partisi: **min_spiffs**, app0/app1 sekitar 1,9 MB. `partitions.csv` di
  folder sketch dipakai otomatis. Layout 4 MB aman untuk flash 4 MB; board
  16 MB tetap berjalan.
- Flash USB pertama wajib setelah tabel partisi berubah. Update berikutnya
  bisa lewat OTA HTTP.
- NVS berisi preset, kalibrasi, pengaturan, tangki, dan runstate; tidak
  hilang saat flash ulang. Gunakan erase flash hanya bila data NVS korup.

## 3. Menu dan Tombol

| Menu | Fungsi |
|---|---|
| Pilih Preset | Daftar preset, mulai, jeda, lanjut, batal |
| Konfigurasi Preset | Tambah, duplikat, hapus, edit 19 field |
| Level Cairan | Status lima sensor, kalibrasi 0%/100% |
| Kalibrasi | Motor, sensor level, suhu, udara, diagnostik E-stop |
| Pengaturan | Suhu, margin drain, kestabilan level, MQ-135, kapasitas tangki |
| Isi Ulang Tangki | Isi volume awal HCl/H2O2/H2O/sabun/limbah |
| Status IoT | WiFi, IP, RSSI, MQTT, broker, TLS, NTP, OTA, heap |
| Info/Status | Versi, suhu, level, MQ-135, alasan reset |

Tombol:

- **ATAS/BAWAH** - navigasi atau scroll.
- **KIRI/KANAN** - ubah nilai; auto-repeat pada beberapa halaman edit.
- **ENTER** - pilih, konfirmasi, ON/OFF, atau mulai.
- **BACK** - kembali. Tahan **1 detik** untuk kembali ke menu utama; tidak
  berlaku saat proses, error/E-stop, atau dialog pemulihan.

Halaman **Status IoT** refresh 500 ms dan bisa di-scroll. Baris: WiFi,
SSID, IP, RSSI, MQTT, Host, Port/TLS, Device, Base, NTP, OTA, Reason,
Heap. Hasil OTA terakhir disimpan di NVS.

## 4. Preset v3

Preset menyimpan 19 field:

1. PCB panjang dan lebar (mm)
2. Mode rasio: persen atau ml
3. HCl, H2O2 (ml atau persen); H2O dihitung otomatis
4. Volume total campuran (ml)
5. Isi tembaga (% luas PCB yang dilarutkan)
6. Tebal tembaga (oz; internal 1 oz = 35 um)
7. Laju etch (um/menit, tampilan 0,1)
8. Mode waktu: manual atau otomatis
9. Lama etching per siklus (detik)
10. Speed agitasi: SPD1/SPD2/SPD3
11. Jumlah siklus penuh (1-6)
12. Bilas sabun per siklus (0-3)
13. Bilas air per siklus (0-5)
14. Volume tiap bilas (ml)
15. Agitasi tiap bilas (detik)
16. Konfirmasi di akhir siklus (ON/OFF)

Waktu etching otomatis:

```text
detik = (tebalCu_um / lajuEtch_um_per_menit) x 60 x 150% / siklus
```

Hasil dibatasi 30-1800 detik per siklus. Faktor 150% adalah margin aman
bawaan. `isiTembagaPct` tidak mengubah waktu; nilainya dipakai untuk
estimasi beban tembaga dan peringatan kapasitas larutan (120 g Cu/L).

Field `tebalCu` di layar memakai oz dengan langkah 0,5 oz (0,5-6,0 oz).
1 oz = 35 um. `lajuEtch` harus diukur per mesin; nilai nominal 3,0 um/menit.

## 5. Alur Proses

1. Pilih preset, lalu **ENTER**.
2. **PRECHECK** memeriksa:
   - kalibrasi pompa yang dipakai,
   - sensor wajib,
   - kebutuhan total seluruh proses per jerigen,
   - kapasitas limbah untuk seluruh cairan,
   - kecukupan tembaga terhadap kapasitas larutan.
3. Setelah precheck lulus, level kering atau hampir penuh hanya menjadi
   peringatan. Limbah kritis (>=95% selama 5 detik) tetap menghentikan proses.
4. **WAIT_PCB** - pasang PCB, lalu **ENTER**.
5. Satu siklus penuh:
   - dosing HCl, H2O2, H2O (satu pompa pada satu waktu),
   - agitasi/etching sesuai `agitasiDetikEfektif`,
   - drain,
   - bilas sabun sebanyak `bilasSabun`,
   - bilas air sebanyak `bilasAir`,
   - drain akhir.
6. Siklus diulang sebanyak `siklus`. Bilas dihitung **per siklus**, bukan
   total di akhir.
7. Jika konfirmasi aktif, layar menawarkan **Siklus lagi** atau **Selesai**.
8. Selesai: wadah sudah kosong; ENTER/BACK kembali.

Selama proses:

- **ENTER** - jeda/lanjut. Jeda menghentikan stepper, DC, agitasi, dan timer.
- **BACK** - dialog batal. Pilihan:
  - **Lanjutkan**
  - **Batalkan + drain**
  - **Abaikan (stop)** - menghentikan proses tanpa drain dan kembali ke menu.
- **E-stop** - semua aktuator mati, lalu pompa limbah mengevakuasi cairan
  bila nampan berisi. Reset menunggu evakuasi selesai.
- Layar dosing menampilkan volume yang sudah masuk, misalnya
  `HCl 45/120 ml`, `AIR 62/180 ml`, `SABUN 20/100 ml`.

Drain memakai ledger `volumeWadah`:

- volume dicatat setelah dosis benar-benar masuk dan runstate disimpan,
- residu 50 ml,
- margin 25% + purge 5 detik,
- laju minimum 3 ml/detik,
- BACK pada dialog drain = **Kosong** (hapus ledger).

Jika alat mati di tengah proses, boot berikutnya menawarkan:

- **Drain & bilas**
- **Drain saja**
- **Abaikan (stop)**

Tidak ada aktuator yang menyala otomatis sebelum pilihan diambil.

## 6. Kalibrasi

Kalibrasi disimpan di NVS dan langsung dipakai dosing/drain. Lakukan dengan
air sebelum memakai larutan asli. Dashboard hanya menampilkan hasil dan
tanggal terakhir; proses kalibrasi tetap dari panel fisik.

### 6.1 Pompa stepper (HCl, H2O2, air)

Target tetap 100 ml.

1. Isi jeriken, priming 10-15 detik ke wadah buang.
2. Pindahkan selang ke gelas takar kosong/kering.
3. START 100 ml; pompa memakai model tersimpan atau base nominal.
4. Ukur hasil, isi **Hasil**, ENTER.
5. Ulangi minimal 3 run sampai **LULUS** (error <=5%).
6. Jika muncul `Cek priming!`, periksa selang, udara, atau sambungan.

Rumus koreksi: `a_baru = langkah / hasil`, `b_baru = 0`.

### 6.2 Pompa DC (sabun dan limbah)

1. Atur duty dan target.
2. Run pertama setiap sesi memakai pulse pendek: sabun 5 detik, limbah
   3 detik. Ini mencegah overshoot saat laju belum diketahui.
3. Ukur hasil, lalu jalankan target 100 ml.
4. Model dikoreksi `r = hasil / detik perintah`, `c = 0`.
5. Minimal 3 run dan error <=10% untuk **LULUS**.

Limbah tidak wajib dikalibrasi untuk memulai proses; drain memakai fallback
8 ml/detik + margin. Sabun wajib jika preset memakai bilas sabun.

### 6.3 Sensor level

Simpan 0% saat kering dan 100% saat terendam. Status proses memakai
histeresis: basah >=55%, kering <=45%. Nilai di luar rentang wajar menjadi
FAULT (E03).

### 6.4 Suhu, udara, agitasi, E-stop

- **Suhu**: pilih blok sensor, alamat ROM, dan offset.
- **Udara**: rekam acuan bersih/kotor; hanya dipakai bila MQ-135 ON.
- **Agitasi**: atur duty SPD1/SPD2/SPD3.
- **E-stop**: halaman diagnostik menampilkan DITEKAN/LEPAS, level GPIO8,
  dan state safety secara live.

## 7. Parameter Default

| Parameter | Nilai |
|---|---|
| Kapasitas HCl/H2O2/H2O | 1000 ml per jerigen |
| Kapasitas sabun | 600 ml |
| Kapasitas limbah | 5000 ml |
| Batas volume preset | 50-1000 ml |
| RPM stepper | 400-700, default 500 |
| Akselerasi stepper | 500 rpm/detik |
| Langkah/putaran | 3200 (DRV8825 1/16) |
| Suhu peringatan / bahaya | 45 C / 60 C |
| Margin drain | 25% + 5 detik purge |
| Residu drain | 50 ml |
| Laju drain minimum | 3 ml/detik |
| Limbah penuh | >=85% selama 3 detik; reset 75% |
| Grace sensor limbah | 2 detik setelah pompa drain |
| Level basah / kering | 55% / 45% |
| Watchdog loop | 15 detik |
| Tahan BACK | 1 detik |
| Toleransi kalibrasi | stepper 5%, DC 10% |
| Laju etch default | 3,0 um/menit |
| Faktor aman waktu | 150% |
| Kapasitas tembaga | 120 g/L |

Preset default berjumlah 5. Rasio awal 33:17:50; preset dapat diubah lewat
**Konfigurasi Preset**. Preset default disesuaikan agar kebutuhan air satu
proses tidak melebihi kapasitas jerigen.

## 8. Pelacak Tangki dan PRECHECK

Menu **Isi Ulang Tangki** menyimpan volume awal HCl, H2O2, H2O, sabun, dan
limbah. Setiap dosing mengurangi sumber dan menambah limbah; drain menambah
limbah. PRECHECK membandingkan seluruh kebutuhan proses dengan sisa sumber
dan ruang limbah.

- Sumber kurang: **E05**, pesan menyebut item dan angka, misalnya
  `HCl: butuh 240, sisa 100`.
- Limbah tidak cukup: **E06**, misalnya `Limbah perlu 1980, ruang 800`.
- Saat alat **IDLE** dan safety AMAN, firmware menyinkronkan ledger setiap
  2 detik dari sensor level terkalibrasi:
  `volume = kapasitas x persen / 100`. Jadi isi ulang manual langsung
  tercermin di layar Tangki dan web tanpa membuka menu.
- Sinkronisasi otomatis berhenti saat proses/jeda/WAIT_PCB/pemulihan/evakuasi.
  Selama proses, ledger dosing/drain tetap menjadi acuan.
- Ambang perubahan 3% kapasitas (minimum 30 ml) agar noise sensor tidak
  menulis NVS terus-menerus. Sensor harus terkalibrasi dan tidak FAULT.
- Nilai sensor adalah estimasi (sensor titik), bukan pengukuran volume linier.
  Menu Tangki tetap bisa dipakai untuk nilai pasti (ENTER = penuh,
  BACK = kosong).

## 9. Error dan Tindakan

| Kode | Nama | Arti | Tindakan |
|---|---|---|---|
| E01 | ESTOP | Tombol darurat ditekan | Lepas tombol, clear lokal/dashboard |
| E02 | TEMP_HIGH | Suhu bahaya | Dinginkan, cek sensor/kipas |
| E03 | SENSOR | Sensor wajib gagal | Cek wiring dan kalibrasi |
| E04 | TIMEOUT | Langkah melebihi batas | Cek pompa/selang, clear, ulangi |
| E05 | TANK | Sumber kosong/kurang | Isi ulang, clear |
| E06 | WASTE | Limbah penuh/tidak cukup | Kosongkan limbah, clear |
| E07 | NVS | Data NVS rusak | Isi ulang data/reset |
| E08 | RESUME | Proses terputus | Pilih drain/bilas/abaikan |
| E09 | CAL | Kalibrasi pompa belum lengkap | Kalibrasi pompa yang ditandai |
| E10 | WDT | Watchdog reset | Cek log dan beban loop |

Saat fault aktif, tombol proses diblok dan aktuator dimatikan. Buzzer
memakai pola error. E-stop hanya bisa di-clear setelah tombol fisik lepas;
dashboard boleh clear E-stop jika status `clear_remote=true`. Error
non-E-stop tetap harus di-clear dari antarmuka lokal.

## 10. Penyimpanan NVS

Namespace: `etch`.

| Key | Isi |
|---|---|
| `cfg` | Preset v3 + checksum |
| `kal` | Kalibrasi level |
| `pompa` | Kalibrasi stepper/DC/agitasi |
| `kal2` | Suhu, level tambahan, MQ-135 |
| `set` | Pengaturan global (v2) |
| `tank` | Ledger volume tangki |
| `runstate` | Status proses untuk pemulihan |
| `net` | WiFi/MQTT/device/topic |
| `otastat` | Hasil OTA terakhir (0/1/2) |
| `kalts` | Epoch kalibrasi terakhir per pompa/agitasi |

Migrasi preset v1/v2 ke v3 berjalan otomatis. Data korup ditolak dan
dikembalikan ke default dengan log Serial.

## 11. IoT dan OTA

- WiFi default: `PCB Expres V3` / `<WIFI_PASSWORD>`. SSID harus sama persis;
  jalankan `wifiscan` lalu `wifi <ssid> <pass>` bila nama AP berbeda
  (misalnya `PCB Expres V3` satu `s`).
- Broker: `mqtt.loggerindonesia.com:8883`, TLS, CA ISRG Root X2.
- Topik: `pelarut/<deviceId>/{online,status,telemetry,event,ack,cmd}`.
- Remote command hanya supervisory; `start` berhenti di `WAIT_PCB`.
- `clear` E-stop dari MQTT hanya setelah tombol fisik HIGH dan evakuasi selesai;
  error non-E-stop tetap lokal.
- Event log 32 entri; epoch dari NTP; replay 5 event setelah reconnect.
- OTA HTTP: `POST http://<IP>:3232/update`, header
  `X-OTA-Password: <OTA_PASSWORD>`, hanya saat IDLE dan safety AMAN.
- Dashboard: **Settings → OTA Firmware**, drag/drop/pilih `.bin`. Server
  dashboard meneruskan file; password tetap di server. Versi/build firmware
  di Settings diperbarui otomatis setelah reboot.

Detail payload dan contoh `mosquitto_pub/sub`: [`IOT-API.md`](./IOT-API.md).
Langkah pengujian: [`PANDUAN-TEST-IOT.md`](./PANDUAN-TEST-IOT.md).

Dashboard web tersedia di [`webapp/`](./webapp/README.md). Browser hanya
terhubung ke server dashboard; server memegang kredensial MQTT. Halaman:
Dashboard, Events, **Preset &amp; Kalibrasi**, dan Settings. Preset &amp;
Kalibrasi memuat editor preset, hasil kalibrasi semua motor/sensor, lalu isi
tangki. Settings memuat versi dashboard/firmware, tanggal rilis, dan repo
GitHub. Header alat memakai nama **Auto Etching V1**; panel kiri bisa
diminimize (default expand).

## 12. Checklist Sebelum Dipakai

Dengan air, bukan larutan asli:

- [ ] Boot normal, versi v1.3.1, tidak reset berulang.
- [ ] Semua menu terbuka; preset edit bertahan setelah restart.
- [ ] Kalibrasi stepper/DC lulus minimal 3 run.
- [ ] Test Motor: semua motor start/stop dengan ramp.
- [ ] Proses dengan air: jeda, lanjut, beberapa siklus, selesai.
- [ ] Cancel/E-stop: motor mati, drain berjalan, tidak ada genangan.
- [ ] Cabut listrik di tengah proses: layar pemulihan muncul.
- [ ] Level kering, limbah penuh, suhu tinggi memunculkan error yang benar.
- [ ] WiFi/NTP/TLS/MQTT dan halaman Status IoT sesuai.
- [ ] OTA hanya saat IDLE; OTA saat proses ditolak.
- [ ] E-stop tetap bekerja dengan WiFi aktif.

Dengan larutan asli: gunakan APD, ventilasi/exhaust, volume kecil, dan
periksa kebocoran/korosi setelah pengujian.

## 13. Keamanan

- Jangan membuka katup atau memindahkan selang saat pompa berjalan.
- HCl/H2O2 korosif; hindari kontak kulit/mata dan uap.
- Exhaust fan harus menyala sebelum larutan dipakai.
- Jangan mengandalkan WiFi/MQTT untuk berhenti darurat; E-stop fisik wajib.
- Kredensial di firmware/NVS masih plaintext; jangan publikasikan build
  dengan kredensial asli.
