# Log Perubahan

Format: `[tanggal] vX.Y.Z - ringkasan`. Rilis terbaru berada di atas.
Registry versi, rollback, dan prosedur tag ada di [`RELEASES.md`](./RELEASES.md).

## [2026-11-11] v1.3.1 - Sinkron tangki otomatis, perbaikan ACK, dan deployment

- **Sinkron tangki otomatis**: saat alat IDLE dan safety AMAN, ledger tangki
  diperbarui tiap 2 detik dari sensor terkalibrasi
  (`kapasitas x persen / 100`). Isi ulang manual langsung terlihat di web.
  Tidak berjalan saat proses/jeda/WAIT_PCB/pemulihan/evakuasi atau edit Tangki.
- **ACK command**: ID command server diperpendek menjadi 16 karakter agar
  cocok dengan buffer firmware (sebelumnya UUID 36 karakter terpotong;
  menyebabkan ACK timeout/502).
- `/api/me` mengembalikan 200 saat belum login (bukan 401) dan boot React
  tidak berjalan ganda di StrictMode.
- Vite dev server listen `0.0.0.0:5173` agar bisa dibuka dari HP di LAN.
- Sidebar desktop sticky; konten utama saja yang scroll.
- Tambah `DEPLOYMENT.md` (file lokal + daftar kredensial tanpa nilai) dan
  `LICENSE` MIT.
- Release source-only: binary firmware tidak disertakan karena hasil compile
  mengandung `secrets.h`.

**Build v1.3.1:** 1.246.654 byte flash (63% dari 1.966.080), RAM 78.260 byte
(23%). Lisensi MIT.

## [2026-11-11] v1.3.0 - OTA dari dashboard, kalibrasi lengkap, dan struktur src

- **OTA dari webapp**: Settings -> OTA Firmware menerima file `.bin` hasil
  compile Arduino (drag/drop atau pick). Browser mengirim ke server; server
  meneruskan ke `http://<IP_ALAT>:3232/update` dengan password OTA server-side.
- Hanya admin, alat online, IDLE, dan safety AMAN. IP dibatasi ke LAN privat.
- Status firmware menambah `ip`, `build`, dan `rssi`; versi/build di Settings
  otomatis berubah setelah alat reboot dan status MQTT baru diterima.
- Settings menampilkan versi dashboard/firmware, build, tanggal rilis, IP,
  repo GitHub, role, broker, dan info koneksi.
- Halaman **Preset & Kalibrasi**: editor preset 19 field, hasil kalibrasi
  stepper/DC/agitasi/level/suhu/MQ-135 + tanggal terakhir, lalu isi tangki.
- Panel kiri hover expand/collapse; default expand.
- Nama alat **Auto Etching V1** pada header, login, dan PWA.
- Header firmware dipindah ke `src/`; `.ino` mengarah ke `src/*.h`.
- Build firmware naik ke v1.3.0; dashboard v1.3.0.

## [2026-11-11] v1.2.0 - Preset v3, proses terukur, IoT, OTA, dan Status IoT

### Proses dan preset

- Preset v3 dengan 19 field: rasio kimia dalam persen/ml, isi tembaga,
  tebal tembaga (oz), laju etch, dan mode waktu manual/otomatis.
- Waktu etching otomatis:
  `(tebalCu / lajuEtch) x 60 x 150% / siklus`, dibatasi 30-1800 detik.
- Kapasitas jerigen HCl/H2O2/H2O 1000 ml dan sabun 600 ml.
- PRECHECK menghitung kebutuhan **seluruh proses** per jerigen dan ruang
  limbah, termasuk bilas x siklus.
- `isiTembagaPct` dipakai untuk estimasi beban tembaga dan peringatan
  kapasitas larutan (120 g Cu/L).
- Layar proses menampilkan progres dosing (`HCl 45/120 ml`, `AIR ...`,
  `SABUN ...`).

### Drain, jeda, dan pemulihan

- Drain memakai ledger `volumeWadah`: volume dicatat setelah dosis masuk,
  residu 50 ml, margin 25%, purge 5 detik, minimum 3 ml/detik, dan
  fallback 8 ml/detik bila pompa limbah belum dikalibrasi.
- Cancel/E-stop memakai sisa volume aktual, bukan mengulang target penuh.
- Opsi jeda: **Lanjutkan / Batalkan + drain / Abaikan (stop)**.
- Opsi pemulihan: **Drain & bilas / Drain saja / Abaikan (stop)**.
- `Abaikan (stop)` benar-benar menghentikan proses tanpa drain dan kembali
  ke menu.
- Runstate disimpan beserta ledger volume, sehingga drain akurat setelah
  restart.
- Dialog drain: **BACK = Kosong** untuk menghapus ledger.

### Keselamatan dan UI

- E01-E10 dengan pesan spesifik, misalnya `E05 HCl: butuh 240, sisa 100`
  atau `E06 Limbah perlu 1980, ruang 800`.
- E-stop latch, watchdog 15 detik, evakuasi cairan, dan blokir aktuator.
- Limbah penuh memakai ambang 85% selama 3 detik, histeresis 75%, grace
  2 detik setelah pompa drain. Ini memperbaiki E06 palsu.
- Tone buzzer berbeda untuk notifikasi, jeda, resume, stop, batal, error,
  dan selesai.
- Layout opsi jeda/pemulihan dan halaman Tangki diberi jarak + scroll.
- Menu Kalibrasi -> **E-Stop** menampilkan status tombol, GPIO8, dan safety.
- Menu utama menjadi 8 item, termasuk **Status IoT**.

### API dan MQTT

- `api.h`: `StatusSnapshot`, `perintahProses()` sebagai satu pintu command,
  event log 32 entri, parser Serial non-blocking.
- `net.h`: WiFi + `ESP32MQTTClient` background task, TLS 8883 dengan CA
  ISRG Root X2, NTP/epoch, LWT, topik `pelarut/<deviceId>/...`, command
  queue + ACK, remote config IDLE-only, rate limit 200 ms, replay 5 event.
- `clear` E-stop dari dashboard hanya setelah tombol fisik lepas; error
  non-E-stop tetap lokal. `start` berhenti di `WAIT_PCB`, lalu `pcb` atau
  ENTER lokal melanjutkan.
- Remote config: tank, setting, preset dengan validasi.
- WiFi: SSID default `PCB Expres V3`; password, broker, dan OTA password
  dipindahkan ke `secrets.h` lokal yang di-ignore Git. NVS lama dengan
  SSID `PCB Express V3` atau `PCB` dimigrasikan otomatis.
- Parser `wifi <ssid> <pass>` memakai spasi terakhir agar SSID ber-spasi
  tidak terpotong.
- Command `wifiscan` menampilkan SSID/RSSI/channel; `wifi default`
  mengembalikan default; `net` menampilkan `ssid`, `wifi_status`, dan
  `wifi_reason`.

### OTA dan Status IoT

- `ota.h`: OTA HTTP ringan (`Update` + `WiFiServer`), port 3232,
  `POST /update`, header `X-OTA-Password: <OTA_PASSWORD>`, hanya saat IDLE dan
  safety AMAN. Menggantikan ArduinoOTA dan menghemat sekitar 46 KB flash.
- Menu **Status IoT**: WiFi/SSID/IP/RSSI, MQTT, host/port/TLS, device,
  base topic, NTP, hasil OTA terakhir, reason, dan heap; refresh 500 ms,
  scroll ATAS/BAWAH.
- Hasil OTA terakhir disimpan di NVS dan tetap terlihat setelah reboot.

### Dashboard web

- `webapp/`: backend Fastify self-hosted + frontend React/Vite.
- Browser hanya HTTPS/WebSocket ke server; kredensial MQTT hanya di server.
- Login lokal, role viewer/operator/admin, CSRF, rate limit, audit SQLite.
- MQTT bridge subscribe status/telemetry/event/ack/online/presets + command/ACK.
- Firmware menambah command `preset`, `pcb`, `presets`, dan `clear` remote
  terkondisi; daftar preset lengkap dipublikasikan retained.
- Preset 19 field bisa dilihat/diedit dari dashboard, disimpan ke NVS alat;
  preset bisa dipilih sebelum start.
- Konfirmasi PCB dari dashboard (`pcb`) saat `WAIT_PCB`.
- Clear E-stop dari dashboard hanya setelah tombol fisik HIGH dan evakuasi
  selesai; error non-E-stop tetap lokal.
- Halaman **Preset & Kalibrasi**: editor preset, hasil kalibrasi stepper/DC/
  agitasi/level/suhu/MQ-135 + epoch terakhir (`kalts`), lalu isi tangki.
- Settings menampilkan versi dashboard/firmware, build, tanggal rilis, IP,
  repo GitHub, dan info koneksi. Header alat memakai **Auto Etching V1**.
- OTA dari dashboard: drop/pick `.bin` di Settings; server meneruskan ke alat.
  Status firmware menambah `ip`, `build`, dan `rssi` agar versi OTA terbaru
  tampil otomatis.
- Header firmware dipindah ke `src/`; `.ino` mengarah ke `src/*.h`.
- Status stale/offline mematikan kontrol; panel kiri bisa minimize.
- UI desktop/mobile/PWA; config tank/setting IDLE-only.

### Build dan partisi

- Firmware: **1.246.234 byte** (94% partisi default 1,25 MB; 63% dari
  1,9 MB `min_spiffs`), RAM **78.252 byte (23%)**.
- `partitions.csv`: 4 MB `min_spiffs`, app0/app1 sekitar 1,9 MB.
- Flash USB pertama wajib setelah perubahan tabel partisi; update
  berikutnya lewat OTA HTTP.
- Catatan: validasi hardware (WiFi/NTP/TLS/MQTT, EMI/power/heat, OTA)
  masih menunggu pengujian di alat.

## [2026-11-10] v1.1.0 - Preset v3 dan waktu etching otomatis

- Rasio kimia bisa persen atau ml; H2O dihitung otomatis.
- Waktu etching otomatis dari tebal tembaga, luas tembaga, dan laju etch.
- PRECHECK memakai kapasitas jerigen 1 L (sabun 600 ml).
- Tebal tembaga diedit dalam oz.
- Migrasi NVS `cfg` v2 -> v3; preset lama tetap memakai mode manual.
- Perbaikan E06 palsu dan progres dosing di layar proses.

## [2026-11-09] v1.0.1 - Siklus penuh berulang dan evakuasi cairan

- Satu siklus penuh = dosing + agitasi + drain + bilas sabun/air, diulang
  sesuai `siklus`.
- Kalibrasi DC memakai pulse pendek pada run pertama, lalu target 100 ml.
- Drain memakai fallback 10 ml/detik + margin bila limbah belum dikalibrasi.
- Cancel proses langsung drain; E-stop/ERROR mengevakuasi cairan dan
  menahan reset sampai selesai.
- UI kalibrasi DC, konfirmasi siklus, dan overlay safety diperbarui.

## [2026-11-05] v1.0.0 - Mesin standalone lengkap

- Preset v2: siklus, konfirmasi, bilas sabun/air.
- State machine proses baru dengan timeout dan pelacak volume.
- Pemulihan setelah listrik padam (`runstate`).
- Modul `safety.h`: E-stop interrupt/latch, E01-E10, watchdog 15 detik.
- Kalibrasi model `langkah = a x ml + b` dan `ml = r x detik + c`.
- UI proses, pengaturan, tangki, dan info baru; tahan BACK 1 detik.

## Riwayat lebih lama

Tag lama tetap tersedia di Git; tidak ada folder arsip di repo.

| Versi | Ringkasan |
|---|---|
| v0.6.6 | Verifikasi hasil kalibrasi LULUS/GAGAL |
| v0.6.5 | Kalibrasi stepper hanya menghitung rotasi di RPM target |
| v0.6.4 | Uji kalibrasi target ml per motor |
| v0.6.3 | Test Motor menampilkan RPM aktual |
| v0.6.2 | Step/rev 3200 (DRV8825 1/16) |
| v0.6.1 | Ramp stepper 18,75 rpm/detik |
| v0.6.0 | Ramp semua motor, Test Motor 7 baris |
| v0.5.4 | Ramp naik/turun stepper |
| v0.5.3 | Halaman Test Motor |
| v0.5.2 | Pompa stepper berbasis RPM 400-700 |
| v0.5.1 | Exhaust fan langsung 12 V |
| v0.5.0 | Baseline modular (menu, preset, dosing, sensor) |
