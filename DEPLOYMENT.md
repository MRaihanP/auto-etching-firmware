# Deployment dan Kredensial

Panduan ini menjelaskan file apa yang **tidak disimpan di repository/release**,
kredensial apa yang harus diisi saat deploy, dan urutan deploy firmware +
webapp. Tidak ada nilai rahasia di dokumen ini.

## 1. File yang Tidak Di-push

File berikut sengaja di-ignore Git dan tidak disertakan dalam GitHub Release:

| File | Isi | Template yang tersedia |
|---|---|---|
| `secrets.h` | Kredensial firmware | `secrets.example.h` |
| `webapp/server/.env` | Konfigurasi + kredensial server webapp | `webapp/server/.env.example` |
| `webapp/server/data/dashboard.db` | SQLite user/session/audit | Dibuat oleh `pnpm seed` |
| `.commandcode/` | Konfigurasi agent lokal | — |
| `node_modules/`, `dist/`, `build/` | Dependency/build output | Dibuat saat build |
| `*.bin`, `*.elf`, `*.map` | Build firmware | Dibuat oleh compiler |
| `*.sqlite`, `*.sqlite-wal`, `*.sqlite-shm` | Database runtime | Dibuat server |
| `*.log` | Log runtime | Dibuat server |

**Penting:** binary firmware hasil compile mengandung `secrets.h`. Jangan
membagikan binary hasil build mesin sendiri. Release source-only harus
di-compile ulang dengan `secrets.h` milik pengguna.

## 2. Kredensial Firmware (`secrets.h`)

Salin:

```bash
cp secrets.example.h secrets.h
```

Isi variabel berikut (tanpa menulis nilainya ke repo):

| Variabel | Fungsi |
|---|---|
| `SECRET_WIFI_SSID` | SSID WiFi alat, harus 2,4 GHz |
| `SECRET_WIFI_PASS` | Password WiFi alat |
| `SECRET_MQTT_HOST` | Host broker MQTT |
| `SECRET_MQTT_PORT` | Port broker (TLS 8883) |
| `SECRET_MQTT_USER` | User MQTT alat |
| `SECRET_MQTT_PASS` | Password MQTT alat |
| `SECRET_MQTT_BASE` | Base topic, default `pelarut` |
| `SECRET_OTA_PASS` | Password endpoint OTA port 3232 |

Jika `secrets.h` tidak ada, firmware tetap compile dengan placeholder, tetapi
tidak akan connect ke WiFi/MQTT.

## 3. Kredensial Webapp (`webapp/server/.env`)

Salin:

```bash
cd webapp
cp server/.env.example server/.env
chmod 600 server/.env
```

Variabel yang perlu diisi:

| Variabel | Fungsi |
|---|---|
| `HOST`, `PORT` | Alamat/port server dashboard |
| `PUBLIC_ORIGIN` | Origin publik dashboard (HTTPS) |
| `COOKIE_SECURE` | `true` jika HTTPS |
| `TRUST_PROXY` | `true` jika di belakang Caddy/Nginx |
| `DB_PATH` | Lokasi SQLite |
| `WEB_DIST` | Lokasi build frontend |
| `MQTT_URL` | URL broker, mis. `mqtts://host:8883` |
| `MQTT_USER`, `MQTT_PASS` | Kredensial MQTT server |
| `MQTT_BASE` | Base topic |
| `DEVICE_ID` | Opsional; kosong = auto-detect |
| `MQTT_CA_PATH` | Opsional CA PEM |
| `OTA_PORT` | Port OTA alat, default 3232 |
| `OTA_PASS` | **Harus sama** dengan `SECRET_OTA_PASS` firmware |
| `OTA_MAX_MB`, `OTA_TIMEOUT_MS` | Batas upload/timeout OTA |
| `SESSION_SECRET` | Secret session lokal |
| `ADMIN_USER`, `ADMIN_PASSWORD` | Admin dashboard; dipakai `pnpm seed` |

Jangan memakai user broker admin/shared untuk dashboard jika broker mendukung
ACL. Buat user dashboard dengan izin subscribe status/telemetry/event/ack/
online/presets dan publish hanya ke `pelarut/<id>/cmd`.

## 4. Urutan Deploy Baru

### Firmware

```bash
cp secrets.example.h secrets.h
# isi secrets.h
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  --libraries /home/raihanP/Arduino/libraries .
~/DevBox/bin/arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" .
```

Flash USB pertama wajib jika tabel partisi berubah. OTA berikutnya lewat
Settings → OTA Firmware atau `POST /update` port 3232.

### Webapp

```bash
cd webapp
pnpm install --prod=false
cp server/.env.example server/.env
chmod 600 server/.env
# isi server/.env
pnpm seed
pnpm build
pnpm start
```

Untuk 24/7 gunakan systemd dari `webapp/deploy/` dan reverse proxy HTTPS.
Pastikan server bisa menjangkau port 3232 alat untuk OTA.

## 5. Kecocokan Kredensial

- `SECRET_WIFI_*` hanya ada di firmware.
- `SECRET_MQTT_*` firmware dan `MQTT_*` webapp harus menunjuk broker yang sama.
  User boleh berbeda jika broker mendukung ACL.
- `SECRET_OTA_PASS` firmware **harus sama** dengan `OTA_PASS` webapp.
- `ADMIN_USER`/`ADMIN_PASSWORD` hanya untuk login web, bukan broker.
- Jangan menaruh password broker di frontend.

## 6. Jika Kredensial Pernah Bocor

1. Rotasi password WiFi, MQTT, OTA, dan admin.
2. Update `secrets.h` dan `webapp/server/.env`.
3. Rebuild/reflash firmware dan `pnpm seed` webapp.
4. Hapus binary/arsip lama yang memuat kredensial lama.
5. Jangan mengandalkan `.gitignore` untuk file yang sudah terlanjur
   di-commit; bersihkan history dan rotasi kredensial.

## 7. Lisensi

Project ini menggunakan [MIT License](./LICENSE).
