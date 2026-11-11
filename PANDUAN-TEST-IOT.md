# Panduan Test IoT — MQTT + TLS + OTA

Panduan ini dipakai setelah firmware final di-flash. Pengujian dilakukan
bertahap: boot → WiFi → NTP → TLS/MQTT → command → remote config → OTA →
hardware.

## 1. Prasyarat

- Board ESP32-S3. Layout `partitions.csv` memakai **4 MB** `min_spiffs`
  (aman untuk flash 4 MB; juga jalan di N16R8/16 MB).
- Library: `ESP32MQTTClient` dan `ArduinoJson` di
  `/home/raihanP/Arduino/libraries`.
- Broker: `mqtt.loggerindonesia.com:8883` (TLS), user `<MQTT_USER>`,
  password `<MQTT_PASSWORD>`, base topic `pelarut`.
- WiFi default: `PCB Expres V3` / `<WIFI_PASSWORD>`.
- Password OTA default: `<OTA_PASSWORD>`.
- Kredensial aktual ada di `secrets.h` lokal (di-ignore Git). Dokumen ini
  memakai placeholder.
- Untuk client MQTT di PC: `mosquitto_pub` / `mosquitto_sub`.

## 2. Build dan flash pertama (USB)

Partisi memakai **`min_spiffs`** (app0/app1 ±1,9 MB) agar OTA dual-app
tersedia di flash 4 MB. `partitions.csv` di folder sketch otomatis dipakai.

```bash
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  --libraries /home/raihanP/Arduino/libraries \
  --output-dir build_ota .

~/DevBox/bin/arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  .
```

Catatan:
- Flash pertama harus USB karena tabel partisi berubah.
- Ukuran firmware saat ini ±1,24 MB (63% dari 1,9 MB app; juga muat di
  partisi default 1,25 MB pada 94%).
- Jika port berbeda, cek `dmesg | tail` atau `ls /dev/ttyACM* /dev/ttyUSB*`.

## 3. Cek boot dan jaringan

Buka Serial Monitor **115200**. Yang diharapkan:

```text
[BOOT] Menu_OLED_6Button v1.3.0
[NET] Config belum ada -> default
[NET] Konfigurasi MQTT ke mqtt.loggerindonesia.com:8883
[NET] Sinkron NTP...
[NET] MQTT task dimulai
[NET] MQTT terhubung, subscribe cmd
```

Kirim:

```text
net
```

Target:

```json
{"ok":true,"cmd":"net","enabled":true,"started":true,
 "mqtt_started":true,"wifi":true,"wifi_status":3,"wifi_reason":0,
 "ssid":"PCB Expres V3","mqtt":true,
 "ip":"192.168.x.x","rssi":-60,
 "host":"mqtt.loggerindonesia.com","port":8883,"tls":true,
 "device":"esp32-XXXXXX","base":"pelarut"}
```

Jika WiFi gagal, cek `ssid` (harus sama persis), `wifi_status`, dan
`wifi_reason`. Scan AP 2,4 GHz di sekitar alat:

```text
wifiscan
```

`wifiscan` menampilkan SSID + RSSI + channel. ESP32-S3 **hanya 2,4 GHz**;
SSID yang hanya 5 GHz tidak akan muncul.

Set ulang SSID/password (SSID boleh mengandung spasi; password adalah
token terakhir):

```text
wifiscan
wifi PCB Expres V3 <WIFI_PASSWORD>
```

Gunakan SSID persis yang muncul di `wifiscan`. Password aktual ada di
`secrets.h`.

Jika `mqtt_started:false`, TLS belum dimulai karena waktu NTP belum valid.
Cek internet/NTP. Fallback plain TCP:

```text
mqtt mqtt.loggerindonesia.com 1883 <MQTT_USER> <MQTT_PASSWORD>
# reboot
```

## 3a. Status IoT di OLED

Menu utama → **Status IoT** (item ke-7). Halaman refresh tiap 500 ms dan
bisa di-scroll dengan ATAS/BAWAH. Baris yang ditampilkan:

| Baris | Arti |
|---|---|
| `WiFi` | `OK`, `NO_SSID`, `FAIL`, `LOST`, `DISC`, `IDLE`, `ERR` |
| `SSID` | nama WiFi aktif |
| `IP` | IP alat dari DHCP |
| `RSSI` | kekuatan sinyal (dBm) |
| `MQTT` | `OK` terhubung, `START` task jalan tapi belum connect, `--` mati |
| `Host` | broker MQTT |
| `Port` | `8883 TLS` atau `1883 PLAIN` |
| `Device` | device ID (`esp32-XXXXXX`) |
| `Base` | base topic (`pelarut`) |
| `NTP` | `OK` waktu sudah sinkron, `--` belum |
| `OTA` | `AKTIF`, `OK`, `ERROR`, `SIAP`, atau `--` |
| `Reason` | alasan disconnect WiFi (0 = belum ada) |
| `Heap` | sisa RAM bebas |

Catatan:
- `OTA OK/ERROR` adalah **hasil terakhir**, disimpan di NVS dan tetap
  terlihat setelah reboot.
- `OTA AKTIF` berarti upload sedang berjalan.
- Jika `MQTT START` lama, cek `NTP` dan log Serial TLS.

## 4. Test MQTT dari PC

Siapkan CA root (sekali):

```bash
openssl s_client -connect mqtt.loggerindonesia.com:8883 \
  -servername mqtt.loggerindonesia.com -showcerts </dev/null 2>/dev/null \
  | awk '/BEGIN CERTIFICATE/{n++} n==4{print} /END CERTIFICATE/ && n==4{exit}' \
  > isrg-root-x2.pem
```

Set kredensial MQTT untuk shell (nilai aktual dari `secrets.h`):

```bash
export MQTT_USER='<MQTT_USER>'
export MQTT_PASS='<MQTT_PASSWORD>'
```

Subscribe semua topik perangkat:

```bash
mosquitto_sub -h mqtt.loggerindonesia.com -p 8883 \
  --cafile isrg-root-x2.pem \
  -u "$MQTT_USER" -P "$MQTT_PASS" \
  -t 'pelarut/#' -v
```

Kirim command (ganti `<id>` dengan device ID dari `net`):

```bash
mosquitto_pub -h mqtt.loggerindonesia.com -p 8883 \
  --cafile isrg-root-x2.pem \
  -u "$MQTT_USER" -P "$MQTT_PASS" \
  -t 'pelarut/<id>/cmd' \
  -m '{"id":"t1","cmd":"status"}'
```

Target yang terlihat:
- `pelarut/<id>/online` = `1` retain.
- `pelarut/<id>/status` retained, JSON lengkap.
- `pelarut/<id>/telemetry` tiap ±5 detik.
- `pelarut/<id>/ack` untuk setiap command.
- `pelarut/<id>/event` event baru.

## 5. Test command proses

Urutan aman di bench (tanpa larutan):

| Test | Command | Hasil |
|---|---|---|
| Status | `{"id":"0","cmd":"status"}` | ACK + status publish |
| Daftar preset | `{"id":"p","cmd":"presets"}` | topik `presets` retained dikirim ulang |
| Pilih preset | `{"id":"1","cmd":"preset","preset":0}` | preset aktif berubah, IDLE-only |
| Start | `{"id":"2","cmd":"start","preset":0}` | masuk `WAIT_PCB`, belum ada aktuator |
| Konfirmasi PCB | `{"id":"3","cmd":"pcb"}` atau ENTER lokal | proses mulai |
| Pause | `{"id":"4","cmd":"pause"}` | aktuator berhenti, status `jeda:true` |
| Resume | `{"id":"5","cmd":"resume"}` | proses lanjut |
| Cancel | `{"id":"6","cmd":"cancel"}` | motor lain mati, drain nampan |
| Drain | `{"id":"7","cmd":"drain"}` | hanya saat idle dan nampan berisi |
| Clear E-stop | `{"id":"8","cmd":"clear"}` | hanya jika `safety.clear_remote=true` |

Verifikasi:
- `start` berhenti di `WAIT_PCB`; `pcb` atau ENTER lokal melanjutkan.
- `clear` ditolak saat tombol E-stop fisik masih ditekan; status
  `safety.clear_remote` harus `true`.
- Command saat safety E-STOP/ERROR (selain clear terkondisi) ditolak.
- Nilai `ack.id` sama dengan `id` command.
- Command beruntun <200 ms menghasilkan `rate limit`.

## 6. Test remote config

Hanya saat **IDLE** (tidak ada proses berjalan).

Tank:
```json
{"id":"c1","cmd":"config","payload":{"target":"tank","idx":0,"ml":900}}
```

Setting:
```json
{"id":"c2","cmd":"config","payload":{"target":"setting","name":"drainMarginPct","value":25}}
```

Preset:
```json
{"id":"c3","cmd":"config","payload":{"target":"preset","idx":0,"fields":{"siklus":2,"agitasiDetik":300}}}
```

Target:
- ACK `ok:true` dan nilai berubah di OLED/NVS setelah restart.
- Config saat proses berjalan → `ok:false,"proses berjalan"`.
- Nilai preset tidak valid (mis. `siklus:99`) → ditolak `validasiPreset`.
- `tank` di luar kapasitas → ditolak.

## 7. Test event, epoch, dan reconnect

- Setelah NTP sinkron, event baru memiliki `epoch` > 0.
- Putuskan WiFi sebentar lalu sambungkan:
  - `online` menjadi `0` (LWT) lalu `1`.
  - Maksimal 5 event terakhir dikirim ulang.
- Event penting: E-stop, fault, start/selesai/batal, drain, config, OTA.

## 8. Test OTA

### 8a. OTA dari dashboard (Settings)

1. Build firmware menjadi file `.bin`.
2. Buka **Settings → OTA Firmware** sebagai admin.
3. Drag/drop file `.bin` atau klik **Pilih file .bin**.
4. Klik **Upload OTA**.
5. Server mengirim file ke `http://<IP_ALAT>:3232/update`; browser tidak
   memegang password OTA.
6. Setelah alat reboot, versi/build di Settings berubah otomatis dari status
   MQTT baru.

Syarat dashboard OTA:
- alat online, IDLE, safety AMAN;
- status alat sudah melaporkan `ip` (firmware baru);
- server dashboard dan alat berada di jaringan yang bisa saling menjangkau
  port 3232;
- `OTA_PASS` di `webapp/server/.env` sama dengan password OTA firmware.

### 8b. OTA via curl (diagnostik)

Syarat:
- Alat **IDLE**, safety AMAN, tidak ada proses/evakuasi.
- WiFi terhubung, `otaAktif=false`.
- PC dan alat di jaringan yang sama.
- Password OTA: `<OTA_PASSWORD>` (dari `secrets.h`).

Cek kesiapan di Serial:
```text
[OTA] HTTP siap di port 3232
```

Upload via `curl` (ganti `<IP_ALAT>` dengan IP dari command `net`):

```bash
curl -X POST \
  --data-binary @build_ota/auto-etching-firmware.ino.bin \
  -H "X-OTA-Password: <OTA_PASSWORD>" \
  http://<IP_ALAT>:3232/update
```

Target:
- Respons `OK, restart`.
- Serial menampilkan `[OTA] Mulai, <n> byte`, lalu alat reboot.
- `version`/`ver` di status sesuai firmware baru.
- Password salah → `401 Unauthorized`.
- Alat sedang proses/error → `409 Conflict` (`Alat tidak IDLE/AMAN`).
- Path selain `/update` → `404`.

Catatan:
- OTA HTTP memakai port 3232 dan **IP langsung**; tidak memakai mDNS.
- Jika gagal: cek IP, password OTA, ukuran firmware, dan partisi
  dual-app (`partitions.csv`/`PartitionScheme=min_spiffs`).
- Jangan memulai proses saat upload berjalan; start proses diblokir selama
  `otaAktif=true`.

## 9. Test hardware setelah firmware final

Lakukan dengan air dulu, bukan larutan asli:

- Semua pompa/stepper/agitasi menyala dengan WiFi dan MQTT aktif.
- E-stop lokal mematikan semua aktuator meski WiFi putus.
- Cancel/E-stop mengevakuasi cairan dan tidak menyisakan genangan.
- RSSI, reconnect, heap (`status.heap`), dan brownout stabil.
- Suhu/EMI tidak mengganggu sensor level/touch saat WiFi + motor aktif.
- OTA tidak dijalankan saat proses berjalan.
- Baru setelah itu uji larutan asli dengan APD dan ventilasi.

## 10. Troubleshooting cepat

| Gejala | Cek |
|---|---|
| `mqtt_started:false` | NTP/waktu belum valid; internet; coba fallback 1883 |
| `mqtt:false` | TLS/CA, waktu, kredensial broker, ACL topik |
| `wifi:false`, `wifi_status=1` | SSID tidak ditemukan: salah ketik, 5 GHz, atau di luar jangkauan; jalankan `wifiscan` |
| `wifi_status=4`, `wifi_reason=202` | password salah / auth gagal |
| `wifi_status=4`, `wifi_reason=205` | AP menolak koneksi / jarak |
| `wifi:false` lain | SSID/password, jarak/AP, `wifi` command |
| Command tidak dijawab | topik `cmd` salah, device ID salah, rate limit |
| Config ditolak | proses berjalan, nilai di luar range, preset tidak valid |
| OTA port tidak muncul | bukan IDLE, OTA belum start (WiFi), IP/password salah |
| OTA gagal di tengah | WiFi lemah, ukuran firmware, partisi bukan `min_spiffs` |
| Firmware tidak muat | gunakan `min_spiffs`; optimalkan `LOG_LEVEL`/fitur |

## 11. Checklist ringkas

- [ ] Build dengan `PartitionScheme=min_spiffs` dan library MQTT/JSON.
- [ ] Flash USB pertama, boot normal.
- [ ] WiFi + NTP + TLS + MQTT connected.
- [ ] `online=1`, `status` retained, telemetry 5 s.
- [ ] Command start/pause/resume/cancel/status + ACK.
- [ ] `start` berhenti di WAIT_PCB; clear MQTT ditolak.
- [ ] Remote config tank/setting/preset IDLE-only.
- [ ] Epoch > 0 dan replay 5 event setelah reconnect.
- [ ] OTA idle berhasil; OTA saat proses tidak dilayani.
- [ ] E-stop lokal tetap bekerja dengan WiFi aktif.
- [ ] Uji hardware dengan air sebelum larutan asli.
