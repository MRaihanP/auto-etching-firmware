# IOT-API — Fondasi Command, Status, dan Event

Dokumen ini menjelaskan command API firmware v1.3.1. Transport yang tersedia:
**Serial non-blocking** (115200 baud, satu perintah per baris) dan
**MQTT** (`net.h`, TLS 8883). Keduanya memakai dispatcher yang sama dan
mengirim `StatusSnapshot`; tidak boleh mengakses aktuator langsung.

## Prinsip

- **Local-first**: kehilangan jaringan tidak boleh menghentikan proses.
- **Satu pintu perintah**: `perintahProses()` — validasi safety/state di situ.
- **Remote start**: `start` menyiapkan proses sampai `WAIT_PCB`;
  konfirmasi PCB bisa lewat command `pcb` atau ENTER lokal.
- **Clear E-stop**: command `clear` hanya diterima setelah tombol E-stop
  fisik dilepas (pin HIGH) dan evakuasi cairan selesai. Error non-E-stop
  tetap hanya bisa di-clear dari panel lokal.
- **Non-blocking**: parser membaca maksimal 64 karakter per loop; tidak ada
  `delay()` atau `while` menunggu jaringan.

## Perintah Serial

| Perintah | Fungsi |
|---|---|
| `help` | Daftar perintah |
| `status` | Snapshot JSON lengkap |
| `events` | 32 event terakhir |
| `start [idx]` | Mulai preset (idx 0-based; default preset terpilih) |
| `pause` | Jeda proses |
| `resume` | Lanjutkan proses |
| `cancel` | Batalkan proses (drain otomatis jika ada cairan) |
| `drain` | Drain nampan saat ini |
| `preset <idx>` | Pilih preset (IDLE-only) |
| `pcb` | Konfirmasi PCB sudah di nampan saat WAIT_PCB |
| `clear` | Clear E-stop; hanya jika tombol fisik sudah lepas |
| `presets` | Minta publikasi ulang daftar preset |
| `tank <0-4> <ml>` | Set volume tangki HCl/H2O2/H2O/sabun/limbah |
| `version` | Versi firmware |
| `wifi <ssid> <pass>` | Simpan WiFi dan coba connect (SSID boleh ber-spasi) |
| `wifi default` | Kembalikan SSID/password default firmware |
| `wifiscan` | Scan AP 2,4 GHz: SSID, RSSI, channel |
| `mqtt <host> <port> <user> <pass>` | Ubah konfigurasi broker (reboot untuk apply) |
| `device <id>` | Set device ID / topik perangkat |
| `topic <base>` | Set base topic |
| `net` / `net on` / `net off` / `net save` | Status dan kontrol koneksi |
| `config <json>` | Remote config tank/setting/preset (IDLE-only) |

Contoh:

```text
status
start 0
pause
resume
cancel
tank 0 900
```

## Format status

`status` mengirim satu baris JSON. Field utama:

```json
{
  "ok": true,
  "cmd": "status",
  "ver": "v1.3.1",
  "uptime": 123456,
  "reset": "PWRON",
  "fase": 3,
  "fase_nama": "ETCH",
  "langkah": 5,
  "langkah_nama": "AGITASI",
  "siklus": 1,
  "siklus_total": 2,
  "preset": 0,
  "preset_nama": "Preset 1",
  "volume_ml": 360,
  "jalan": true,
  "jeda": false,
  "tunggu_pcb": false,
  "konfirmasi": false,
  "pemulihan": false,
  "ota_aktif": false,
  "ota_hasil": 0,
  "safety": {
    "state": 0, "kode": 0, "nama": "OK", "detail": "", "aksi": "",
    "pin_high": true, "clear_remote": false
  },
  "dosis": { "nama": "HCl", "target": 120, "sudah": 45 },
  "suhu": [31.2, 30.8, 32.1],
  "suhu_valid": [true, true, true],
  "level": [
    { "p": 90, "basah": true, "fault": false }
  ],
  "tank": [
    { "ml": 900, "kap": 1000 }
  ],
  "kalib": {
    "stepper": [true, true, true],
    "dc": [12.5, 24.0],
    "ml_per_rev": [0.52, 0.49, 0.55],
    "rpm": [500, 500, 500],
    "stepper_epoch": [1767225600, 1767225600, 1767225600],
    "dc_duty": [80, 100],
    "dc_epoch": [1767225600, 1767225600],
    "agitasi_epoch": 1767225600,
    "agitasi_duty": [40, 70, 100],
    "level": [
      { "kosong": 113000, "penuh": 156000, "ok": true }
    ],
    "suhu": [
      { "addr": 1, "offset_x10": 0 }
    ],
    "mq": { "normal": 120, "kotor": 400, "aktif": 1 },
    "level_epoch": 1767225600,
    "suhu_epoch": 1767225600,
    "mq_epoch": 1767225600
  },
  "heap": 250000,
  "heap_min": 240000
}
```

Catatan:
- Log debug firmware memakai port Serial yang sama (`LOG_LEVEL`). Jika ada
  log yang menyela, abaikan baris yang tidak diawali `{`.
- `level` berisi 5 sensor: HCl, H2O2, H2O, sabun, limbah.
- `tank` berisi 5 tangki dengan urutan yang sama.
- `safety.state`: `0=AMAN`, `1=ESTOP`, `2=ERROR`.
- Timestamp event memakai `millis()` dan `epoch`; `epoch` bernilai 0 sampai
  NTP sinkron.
- `ota_hasil`: `0` belum pernah, `1` sukses, `2` error. Nilai ini juga
  ditampilkan di menu **Status IoT** dan disimpan di NVS.

## Event log

Ring buffer 32 entri di RAM. Setiap entri:

```json
{ "t": 123456, "epoch": 1767225600, "kode": 5, "detail": "HCl butuh 240, sisa 100" }
```

Kode event:
- `1` E-stop.
- `2..10` kode fault E02–E10.
- `100` proses mulai.
- `101` proses selesai.
- `102` proses dibatalkan.
- `103` stop darurat.
- `104` drain dimulai.
- `105` evakuasi selesai.
- `106` sinkron tangki dari sensor.
- `107` proses diabaikan/dihentikan.
- `108` pemulihan dijalankan.
- `110..118` perintah API (start/pause/resume/cancel/drain/clear/preset/pcb).
- `120` command MQTT diterima (audit), `121` rate limit, `122` remote setting,
  `123` remote preset.
- `130` OTA mulai, `131` OTA selesai, `132` OTA error.

## Status IoT di OLED

Menu utama -> **Status IoT** (item ke-7). Halaman menampilkan WiFi, SSID,
IP, RSSI, MQTT, host, port/TLS, device, base topic, NTP, status OTA,
reason disconnect, dan free heap. Refresh 500 ms; scroll ATAS/BAWAH.
`OTA OK/ERROR` adalah hasil terakhir yang disimpan di NVS.

## MQTT (Phase 2)

Broker default (dapat diubah lewat Serial):

```text
Host : mqtt.loggerindonesia.com
Port : 8883 (TLS)
User : <MQTT_USER>
Pass : <MQTT_PASSWORD>
Base : pelarut
CA   : ISRG Root X2 (net_ca.h)
```

Library: `ESP32MQTTClient` (background task, non-blocking) dan `ArduinoJson`.
Jika memakai arduino-cli, tambahkan:

```bash
--fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs"
--libraries /home/raihanP/Arduino/libraries
```

`PartitionScheme=min_spiffs` untuk OTA dual-app (app0/app1 ±1,9 MB)
di flash 4 MB. `partitions.csv` di folder sketch otomatis dipakai.
Flash pertama harus USB karena tabel partisi berubah; setelah itu update
bisa lewat OTA.

### Provisioning WiFi

Default WiFi diambil dari `secrets.h` lokal (di-ignore Git):

```text
SSID     : PCB Expres V3
Password : <WIFI_PASSWORD>
```

Clone baru: salin `secrets.example.h` menjadi `secrets.h`, lalu isi nilainya.

Alat akan langsung mencoba connect setelah flash. Untuk mengganti jaringan,
kirim lewat Serial:

```text
wifiscan
wifi PCB Expres V3 <WIFI_PASSWORD>
net
```

`wifi` memakai **spasi terakhir** sebagai pemisah: SSID boleh mengandung
spasi, password tidak boleh mengandung spasi. Jika SSID asli memakai satu
`s` (`PCB Expres V3`), tulis persis seperti hasil `wifiscan`. ESP32-S3
hanya bisa melihat AP **2,4 GHz**.

`net` sekarang menampilkan `ssid`, `wifi_status`, dan `wifi_reason` untuk
diagnosa. `wifi` langsung menyimpan ke NVS dan mencoba connect. Kredensial
ini tersimpan plaintext di NVS; untuk produksi gunakan provisioning aman
(Phase 2). Alternatif:

```text
net on
net off
net save
mqtt <host> <port> <user> <pass>
device <id>
topic <base>
```

Perubahan `mqtt` (host/port/user/pass), `device`, atau `topic` saat MQTT sudah
berjalan memerlukan reboot agar ESP32MQTTClient memakai konfigurasi baru.
Password tidak dicetak pada status.

### Topik

Device ID default dari MAC: `esp32-XXXXXX`.

| Arah | Topik | Isi |
|---|---|---|
| device → broker | `pelarut/<id>/online` | `1` retain saat connect; LWT `0` |
| device → broker | `pelarut/<id>/status` | snapshot JSON lengkap, retain |
| device → broker | `pelarut/<id>/telemetry` | snapshot JSON tiap 5 s |
| device → broker | `pelarut/<id>/event` | event terbaru |
| device → broker | `pelarut/<id>/ack` | hasil command |
| device → broker | `pelarut/<id>/presets` | daftar preset lengkap, retained |
| broker → device | `pelarut/<id>/cmd` | command JSON, QoS1 |

### Command MQTT

```json
{ "id": "cmd-1", "cmd": "start", "preset": 0 }
{ "id": "cmd-2", "cmd": "pause" }
{ "id": "cmd-3", "cmd": "resume" }
{ "id": "cmd-4", "cmd": "cancel" }
{ "id": "cmd-5", "cmd": "drain" }
{ "id": "cmd-6", "cmd": "preset", "preset": 0 }
{ "id": "cmd-7", "cmd": "pcb" }
{ "id": "cmd-8", "cmd": "clear" }
{ "id": "cmd-9", "cmd": "status" }
```

ACK:

```json
{ "id": "cmd-1", "ok": true, "cmd": "start", "msg": "start Preset 1" }
```

### Remote config (IDLE-only)

Kirim lewat topik `cmd`:

```json
{"id":"c1","cmd":"config","payload":{"target":"tank","idx":0,"ml":900}}
{"id":"c2","cmd":"config","payload":{"target":"setting","name":"drainMarginPct","value":25}}
{"id":"c3","cmd":"config","payload":{"target":"preset","idx":0,"fields":{"siklus":2,"agitasiDetik":300}}}
```

Catatan implementasi: command `config` memakai field `payload` berisi objek
konfigurasi. Bisa juga lewat Serial:

```text
config {"target":"tank","idx":0,"ml":900}
```

Validasi:
- Ditolak jika proses sedang berjalan.
- `setting`: suhu, margin drain, stabil level, MQ/ioT, kapasitas tangki.
- `preset`: field divalidasi dengan `validasiPreset()` sebelum disimpan.
- `tank`: 0..kapasitas.
- Semua perubahan disimpan ke NVS.

### NTP dan event

- SNTP (`pool.ntp.org`, `time.google.com`) dimulai sebelum MQTT TLS.
- MQTT TLS baru dimulai setelah waktu sistem valid (>2026-01-01), karena
  validasi sertifikat membutuhkan jam yang benar.
- Event/telemetry menyertakan `epoch`; sampai NTP sinkron nilainya 0.
- Setelah reconnect, maksimal 5 event terakhir dikirim ulang.
- Command dibatasi 200 ms antar-perintah (rate limit) dan dicatat di event log.

Aturan:
- Callback MQTT hanya memasukkan command ke queue. Aktuator tetap dijalankan
  oleh loop utama.
- `start` tetap berhenti di `WAIT_PCB`; konfirmasi PCB lewat `pcb` atau ENTER lokal.
- MQTT task berjalan di background; reconnect tidak memblokir proses.
- `clear` E-stop dari MQTT hanya diterima jika tombol fisik sudah HIGH dan
  evakuasi selesai; error non-E-stop tetap lokal.
- Fallback plain TCP: `mqtt mqtt.loggerindonesia.com 1883 <MQTT_USER> <MQTT_PASSWORD>`
  lalu reboot.

## OTA

- OTA HTTP ringan (`Update` + `WiFiServer`), port **3232**, path `/update`.
- Password header: `X-OTA-Password: <OTA_PASSWORD>` (dari `secrets.h`).
- Hanya dilayani saat alat IDLE dan safety AMAN. Start proses diblokir
  selama `otaAktif=true`.
- Upload:
  ```bash
  curl -X POST \
    --data-binary @build_ota/auto-etching-firmware.ino.bin \
    -H "X-OTA-Password: <OTA_PASSWORD>" \
    http://<IP_ALAT>:3232/update
  ```
- Respons: `200 OK`, `401` password salah, `409` alat tidak IDLE/AMAN,
  `404` path salah.
- Partisi dual-app: `partitions.csv` (4 MB `min_spiffs`, app0/app1 ±1,9 MB)
  atau `PartitionScheme=min_spiffs`.
- Dari dashboard: buka **Settings → OTA Firmware**, drag/drop atau pilih file
  `.bin`. Browser mengirim file ke server dashboard; server meneruskannya ke
  `http://<IP_ALAT>:3232/update` dengan password OTA server-side. Hanya admin,
  hanya saat alat online/IDLE/safety AMAN. Setelah reboot, versi firmware di
  Settings berubah otomatis dari status MQTT baru.
- Panduan langkah lengkap: `PANDUAN-TEST-IOT.md`.

## Yang belum diimplementasikan (langkah IoT final)

1. **Provisioning aman**: captive portal/BLE, NVS encryption, factory reset.
2. **Auth/otorisasi**: kredensial per device, ACL broker per topik,
   pembatasan remote start/clear/calibrate, audit log persisten.
3. **Heap guard**: nonaktifkan IoT jika heap di bawah ambang.
4. **Dashboard/alert** dan validasi EMI/power/heat WiFi + pompa/stepper.
5. **Remote config penuh** untuk pengaturan yang belum tercakup (mis. LED),
   dengan skema JSON berversi.
6. **Rollback/signature OTA** dan larangan update saat proses (saat ini
   pencegahan dilakukan dengan tidak melayani OTA saat proses aktif).
