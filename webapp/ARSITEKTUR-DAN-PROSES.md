# Arsitektur Webapp dan Proses Selanjutnya

Dokumen ini menjelaskan cara kerja dashboard web `webapp/`, proses deployment
lokal 24/7, keamanan, serta jalur belajar web development dari kode yang sudah
ada. Ditulis untuk orang yang belum mendalami web development.

Versi acuan:

- Firmware: **v1.2.0**
- Dashboard: **v1.0.0**
- Tanggal rilis: **2026-11-11**
- Repo: `https://github.com/MRaihanP/auto-etching-firmware`

---

## 1. Gambaran Besar

Sistem ini punya empat bagian:

```text
┌──────────────────────┐
│ Browser / HP         │
│ React + TypeScript   │
└──────────┬───────────┘
           │ HTTPS + WebSocket
           ▼
┌──────────────────────┐
│ Server webapp        │
│ Node.js + Fastify    │
│ auth, API, MQTT      │
│ SQLite, cache        │
└──────────┬───────────┘
           │ MQTT TLS 8883
           ▼
┌──────────────────────┐
│ Broker MQTT          │
│ mqtt.loggerindonesia │
└──────────┬───────────┘
           │ MQTT TLS 8883
           ▼
┌──────────────────────┐
│ Alat ESP32-S3        │
│ firmware etching     │
└──────────────────────┘
```

Istilah sederhana:

| Bagian | Perannya |
|---|---|
| **Browser** | Menampilkan UI; hanya bicara ke server dashboard |
| **Frontend** | Kode React yang berjalan di browser |
| **Backend** | Server Node.js yang menyimpan kredensial MQTT, login, API, audit |
| **Broker MQTT** | Jalur pesan antara server dan alat |
| **Firmware** | Kode di ESP32 yang menggerakkan pompa, sensor, dan safety |
| **Database SQLite** | File lokal untuk user, session, dan audit |
| **Reverse proxy** | Caddy/Nginx yang menangani HTTPS dan meneruskan ke server |

**Aturan keamanan utama:** browser **tidak pernah** menerima kredensial MQTT
dan tidak pernah membuka koneksi ke broker. Semua akses broker dilakukan
server.

---

## 2. Kenapa Ada Backend?

Kalau browser langsung connect ke MQTT, kredensial broker harus dikirim ke
browser. Apa pun yang sampai ke browser bisa dibaca lewat DevTools, network
tab, memory, atau bundle JavaScript. Jadi browser tidak bisa menyimpan rahasia.

Backend menjadi **trusted boundary**:

- menyimpan kredensial broker di file environment server;
- login user dashboard;
- memeriksa role;
- memvalidasi command;
- mengirim command ke MQTT;
- menyimpan audit;
- menyajikan file frontend.

Frontend hanya tahu:

```text
/api/*   → API server
/ws      → WebSocket server
```

Frontend tidak tahu host MQTT, username broker, password broker, atau topik
mentah.

---

## 3. Peta Folder

```text
webapp/
├── package.json                 # script root: dev, build, start, seed
├── pnpm-workspace.yaml          # workspace server + web
├── README.md                    # cara pakai singkat
├── ARSITEKTUR-DAN-PROSES.md     # dokumen ini
│
├── server/                      # BACKEND
│   ├── src/
│   │   ├── index.ts             # entry point Fastify + route API
│   │   ├── config.ts            # baca environment, path, MQTT
│   │   ├── db.ts                # SQLite: user, session, audit
│   │   ├── auth.ts              # login, role, CSRF, cookie
│   │   ├── mqtt.ts              # koneksi broker, cache status, command
│   │   └── seed.ts              # buat/ubah admin pertama
│   ├── data/                    # SQLite runtime (gitignored)
│   ├── .env.example             # template, tanpa rahasia
│   └── .env                     # kredensial lokal (gitignored)
│
├── web/                         # FRONTEND
│   ├── src/
│   │   ├── main.tsx             # mount React
│   │   ├── App.tsx              # boot, login, WebSocket
│   │   ├── api.ts               # wrapper fetch + WebSocket
│   │   ├── store.ts             # state global (Zustand)
│   │   ├── demo.ts              # data UI-only (?demo=1)
│   │   ├── version.ts           # nama alat, versi, repo
│   │   ├── pages/
│   │   │   ├── Login.tsx
│   │   │   ├── Shell.tsx        # sidebar, header, navigasi
│   │   │   ├── Dashboard.tsx
│   │   │   ├── Events.tsx
│   │   │   ├── ConfigPage.tsx   # preset, kalibrasi, tangki
│   │   │   └── Settings.tsx     # versi, repo, info
│   │   └── components/
│   │       ├── ui.tsx
│   │       ├── CalibrationCard.tsx
│   │       └── PresetEditor.tsx
│   └── vite.config.ts           # dev server + proxy + PWA
│
└── deploy/                      # systemd + Caddy contoh
```

---

## 4. Teknologi yang Dipakai

### Frontend

- **React**: membangun UI dari komponen.
- **TypeScript**: JavaScript dengan tipe, membantu mencegah error.
- **Vite**: dev server dan bundler produksi.
- **Tailwind CSS**: utility CSS untuk layout.
- **Zustand**: state global kecil (user, status, event, toast).
- **WebSocket**: server push status ke browser.
- **PWA**: manifest + service worker agar bisa di-install.

### Backend

- **Node.js**: menjalankan JavaScript/TypeScript di server.
- **Fastify**: framework HTTP; routing, plugin, error handling.
- **better-sqlite3**: SQLite sinkron untuk user/session/audit.
- **mqtt**: client MQTT server-side.
- **Zod**: validasi request.
- **scrypt**: hash password bawaan Node.
- **Pino**: logging (bawaan Fastify).

### Infrastruktur

- **Caddy/Nginx**: reverse proxy + HTTPS.
- **systemd**: menjalankan server 24/7.
- **SQLite**: database file lokal.
- **TLS**: HTTPS browser dan MQTT TLS ke broker.

Tidak ada layanan pihak ketiga seperti Auth0, Firebase, Supabase, Vercel,
Netlify, atau Sentry.

---

## 5. Dua Mode Menjalankan Webapp

### 5.1 Development

```bash
cd webapp
pnpm install
pnpm dev
```

Yang terjadi:

- backend berjalan di `http://localhost:8080`;
- Vite berjalan di `http://localhost:5173`;
- Vite mem-proxy `/api` dan `/ws` ke backend;
- browser dibuka ke `http://localhost:5173`.

Mode ini untuk coding. File frontend diubah, browser refresh otomatis.

### 5.2 Produksi

```bash
cd webapp
pnpm install --prod=false
pnpm build
pnpm start
```

Yang terjadi:

- `pnpm build` menghasilkan `web/dist` (HTML/CSS/JS) dan `server/dist`;
- Fastify menyajikan `web/dist` dan API di `http://localhost:8080`;
- tidak ada Vite;
- frontend dan backend satu origin.

### 5.3 UI-only tanpa MQTT

```bash
pnpm --dir web dev
# buka http://localhost:5173/?demo=1
```

atau:

```bash
pnpm --dir web build
pnpm --dir web preview
# buka http://localhost:4173/?demo=1
```

Mode demo memakai data contoh di `web/src/demo.ts`, tidak memanggil API,
tidak membuka WebSocket, dan tidak mengirim command.

---

## 6. Alur Data

### 6.1 Login

```text
Browser
  POST /api/auth/login {username,password}
    → Fastify
      → SQLite: cari user
      → scrypt: verifikasi password
      → buat session token + CSRF
      → Set-Cookie: sid=...; HttpOnly; SameSite=Strict
    ← {user, csrf}
Browser menyimpan user + CSRF di memory
```

- `sid` tidak bisa dibaca JavaScript karena `HttpOnly`.
- CSRF dikirim di header `X-CSRF-Token` untuk POST.
- Session disimpan di SQLite, bukan di browser.

### 6.2 Status awal

```text
Browser
  GET /api/status
    → Fastify auth
      → cache MQTT di memory
    ← {deviceId, status, presets, online, stale, events}
```

### 6.3 Update real-time

```text
Server MQTT bridge
  subscribe status/telemetry/event/ack/online/presets
    → update cache
    → kirim WebSocket ke browser
Browser
  menerima snapshot/telemetry/event/ack
  → Zustand store
  → React render ulang
```

HTTP dipakai untuk request/response. WebSocket dipakai untuk push dari server.
Browser tidak perlu polling terus-menerus.

### 6.4 Command

```text
Browser
  POST /api/commands {cmd:"pause"}
    → cek session + role + CSRF
    → cek status tidak stale
    → server publish MQTT cmd
    → tunggu ACK
    ← {ack}
```

Command tidak dikirim langsung oleh browser ke MQTT.

### 6.5 Preset

- Firmware mempublikasikan `pelarut/<id>/presets` retained.
- Server menyimpan daftar preset di cache.
- Browser menampilkan `PresetEditor`.
- Simpan preset → `POST /api/config` → server publish MQTT `config`.
- Firmware memvalidasi, menyimpan ke NVS, lalu publish ulang `presets`.
- Server menerima daftar baru dan mengirim snapshot ke browser.

### 6.6 E-stop clear

- E-stop fisik ditekan → firmware latch `ESTOP`.
- Status berisi `safety.state=1`, `pin_high`, `clear_remote`.
- Dashboard menampilkan banner merah dan tombol clear hanya jika
  `clear_remote=true`.
- Server memeriksa kondisi yang sama sebelum publish `clear`.
- Firmware memeriksa ulang; clear tetap ditolak jika tombol masih LOW atau
  evakuasi belum selesai.

---

## 7. MQTT yang Dipakai

Topik:

```text
pelarut/<deviceId>/status      retained snapshot
pelarut/<deviceId>/telemetry   snapshot periodik
pelarut/<deviceId>/event       event
pelarut/<deviceId>/ack         hasil command
pelarut/<deviceId>/online      LWT 1/0
pelarut/<deviceId>/presets     daftar preset retained
pelarut/<deviceId>/cmd         command dari server
```

Command:

```json
{"id":"...","cmd":"start","preset":0}
{"id":"...","cmd":"pause"}
{"id":"...","cmd":"resume"}
{"id":"...","cmd":"cancel"}
{"id":"...","cmd":"drain"}
{"id":"...","cmd":"preset","preset":0}
{"id":"...","cmd":"pcb"}
{"id":"...","cmd":"clear"}
{"id":"...","cmd":"config","payload":{...}}
```

---

## 8. Keamanan

### 8.1 Kredensial

- Kredensial MQTT hanya di `webapp/server/.env`.
- File `.env` di-ignore Git; permission `600`.
- Browser tidak pernah menerima host/port/user/password MQTT.
- Admin dashboard terpisah dari user broker.
- Idealnya broker punya user dashboard dengan ACL:
  - subscribe status/telemetry/event/ack/online/presets;
  - publish hanya `pelarut/<id>/cmd`.
- Rotasi kredensial jika pernah bocor.

### 8.2 Login dan role

- Password di-hash scrypt.
- Session `HttpOnly; Secure; SameSite=Strict` saat HTTPS.
- CSRF token untuk semua POST.
- Role:
  - `viewer`: lihat status/event;
  - `operator`: command proses;
  - `admin`: preset, tank, setting, audit.
- Role diperiksa di server, bukan hanya tombol UI.

### 8.3 Transport

- Browser ke server: HTTPS.
- Server ke broker: MQTT TLS 8883.
- Jangan membuka port 1883/8083 ke publik.
- Jangan memakai HTTP untuk akses internet.
- Untuk LAN: Caddy `tls internal`, lalu install CA Caddy di perangkat client.

### 8.4 Batasan fisik

- E-stop fisik tetap jalur keselamatan utama.
- Clear E-stop dari web hanya setelah tombol fisik HIGH.
- Error non-E-stop tetap harus clear lokal.
- `start` berhenti di `WAIT_PCB`; PCB dikonfirmasi via dashboard atau ENTER.
- Dashboard bukan pengganti safety firmware.

---

## 9. Deployment Lokal 24/7

### 9.1 Host

Gunakan mini PC/Raspberry Pi/x86 yang menyala terus. Host harus:

- satu jaringan dengan alat (atau bisa akses broker);
- punya IP LAN tetap;
- bisa keluar ke broker port 8883;
- tidak sleep;
- idealnya UPS.

### 9.2 Dari kondisi sekarang

Saat ini:

- webapp sudah dibuild;
- server lokal sudah dihentikan;
- firmware baru **belum di-flash**.

Urutan selanjutnya:

1. Flash firmware baru via USB.
2. Pastikan alat connect WiFi + MQTT.
3. Siapkan host 24/7.
4. Copy `webapp/` ke host.
5. Buat `/etc/etching-dashboard.env`.
6. `pnpm install --prod=false && pnpm build`.
7. `pnpm seed`.
8. Pasang systemd service.
9. Pasang Caddy/Nginx HTTPS.
10. Akses dashboard dan uji command.

### 9.3 Environment produksi

Contoh `/etc/etching-dashboard.env` (permission `600`):

```text
HOST=127.0.0.1
PORT=8080
PUBLIC_ORIGIN=https://etching.local
COOKIE_SECURE=true
TRUST_PROXY=true
DB_PATH=/var/lib/etching-dashboard/dashboard.db
WEB_DIST=/opt/etching-webapp/web/dist

MQTT_URL=mqtts://mqtt.loggerindonesia.com:8883
MQTT_USER=<MQTT_USER>
MQTT_PASS=<MQTT_PASSWORD>
MQTT_BASE=pelarut
DEVICE_ID=
MQTT_CA_PATH=

ADMIN_USER=admin
ADMIN_PASSWORD=<ADMIN_PASSWORD>
```

`HOST=127.0.0.1` karena reverse proxy yang menghadap LAN. Untuk uji tanpa
proxy, bisa `HOST=0.0.0.0` dan `COOKIE_SECURE=false`, hanya di LAN tepercaya.

### 9.4 systemd

Gunakan contoh `webapp/deploy/etching-dashboard.service`:

```bash
sudo cp deploy/etching-dashboard.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now etching-dashboard
sudo systemctl status etching-dashboard
```

Log:

```bash
journalctl -u etching-dashboard -f
```

### 9.5 Caddy

`webapp/deploy/Caddyfile.example`:

```caddyfile
etching.local {
    tls internal
    encode zstd gzip
    reverse_proxy 127.0.0.1:8080
}
```

Untuk domain publik, gunakan certificate yang dimiliki pengguna. Untuk LAN,
install CA internal Caddy di HP/PC.

### 9.6 Alur operasional

```text
Alat boot → WiFi → MQTT → server melihat online
Browser buka https://etching.local → login
Dashboard menerima status/presets/calibration
```

- Browser ditutup: alat tetap jalan.
- Server mati: alat tetap jalan; command web tidak tersedia.
- Internet putus: proses lokal tetap jalan; dashboard offline/stale.
- Server restart: reconnect broker; retained status/presets dimuat.
- Alat restart: reconnect; dashboard menunggu status baru.
- Tidak ada command otomatis setelah reconnect.

---

## 10. Development untuk Pemula

### 10.1 Urutan membaca kode

1. `web/src/main.tsx` — bagaimana React dipasang.
2. `web/src/App.tsx` — login, boot, WebSocket.
3. `web/src/api.ts` — HTTP request dan WebSocket.
4. `web/src/store.ts` — state global.
5. `web/src/pages/Dashboard.tsx` — komponen UI + command.
6. `web/src/pages/Shell.tsx` — layout/sidebar.
7. `web/src/pages/ConfigPage.tsx` — preset/kalibrasi/tangki.
8. `server/src/index.ts` — route API.
9. `server/src/auth.ts` — session/CSRF/role.
10. `server/src/mqtt.ts` — bridge broker.
11. `server/src/db.ts` — SQLite.

### 10.2 Konsep yang perlu dipelajari

- **HTML/CSS/JS**: struktur, styling, event.
- **TypeScript**: tipe, interface, union, optional.
- **React**: component, props, state, effect, conditional render.
- **Vite**: dev server, build, proxy.
- **HTTP**: method, status, header, cookie.
- **JSON**: format data.
- **REST API**: endpoint + method.
- **WebSocket**: koneksi dua arah untuk push.
- **Async**: `async/await`, Promise, error handling.
- **Auth**: session, cookie, CSRF, role.
- **Database**: table, query, migration.
- **Deployment**: process, port, reverse proxy, TLS, systemd.
- **Security**: secret management, least privilege, XSS/CSRF.

### 10.3 Latihan kecil

1. Ubah nama alat di `web/src/version.ts`; lihat efek di header.
2. Tambah field baru di `CalibrationCard`; pakai data status.
3. Tambah tombol “Refresh status” di halaman Events.
4. Tambah endpoint `GET /api/info` di backend dan tampilkan di Settings.
5. Tambah halaman baru “Tentang” di `Shell.tsx`.
6. Ubah pesan toast command; lihat alur `Dashboard → api → server`.
7. Tambah unit test untuk validasi request (Zod).

### 10.4 Yang tidak boleh dilakukan

- Jangan taruh password MQTT di frontend.
- Jangan commit `.env` atau `secrets.h`.
- Jangan bypass validasi server hanya karena UI sudah disable.
- Jangan menganggap tombol UI sebagai pengganti safety firmware.
- Jangan membuka port MQTT ke internet.
- Jangan memakai `pnpm dev` sebagai service 24/7.

---

## 11. Checklist Pengujian

### Dashboard

- [ ] Login benar/salah, rate limit, session expiry.
- [ ] Role viewer tidak bisa command.
- [ ] Status online/stale/offline tampil benar.
- [ ] Preset list muncul setelah firmware baru.
- [ ] Edit preset → tersimpan → list berubah.
- [ ] Pilih preset → start memakai preset tersebut.
- [ ] PCB confirm saat `WAIT_PCB`.
- [ ] Clear E-stop hanya aktif setelah tombol fisik HIGH.
- [ ] Hasil kalibrasi semua motor/sensor dan tanggal tampil.
- [ ] Isi tangki di bawah kalibrasi.
- [ ] Settings menampilkan versi firmware/dashboard, tanggal, repo.
- [ ] Sidebar hover expand/collapse berfungsi.
- [ ] Mobile 360 px dan desktop 1440 px.

### Server

- [ ] `pnpm build` sukses.
- [ ] `/api/health` OK.
- [ ] SQLite bisa dibackup.
- [ ] systemd restart otomatis.
- [ ] Server reboot → reconnect broker.
- [ ] Browser tidak bisa mengakses broker langsung.
- [ ] `.env` permission 600 dan tidak masuk Git.

### Alat

- [ ] Firmware baru ter-flash.
- [ ] WiFi/MQTT/NTP/TLS terhubung.
- [ ] `presets` retained terkirim.
- [ ] Command start/pause/resume/cancel/drain.
- [ ] E-stop lokal dan clear terkondisi.
- [ ] Kalibrasi tersimpan dan tanggal muncul.
- [ ] OTA hanya saat IDLE.

---

## 12. Proses Update

### Webapp

```bash
cd /opt/etching-webapp
sudo systemctl stop etching-dashboard
pnpm install --prod=false
pnpm build
sudo systemctl start etching-dashboard
```

Backup database sebelum update.

### Firmware

1. Build dengan `min_spiffs`.
2. Flash USB pertama jika tabel partisi berubah.
3. Update berikutnya lewat OTA HTTP port 3232.
4. OTA hanya saat IDLE dan safety AMAN.

---

## 13. Kesimpulan

- Browser adalah lapisan presentasi, bukan pemegang rahasia.
- Server webapp adalah inti: auth, MQTT, cache, audit, API.
- Broker adalah jalur pesan; alat tetap local-first.
- Untuk lokal 24/7, server harus selalu berjalan dan diakses lewat HTTPS.
- Kredensial saja tidak cukup; perlu host + proses + database + reverse proxy.
- Untuk belajar web, mulai dari alur:

```text
main.tsx → App.tsx → api.ts → store.ts → Dashboard.tsx
         → server/index.ts → auth.ts → mqtt.ts → db.ts
```

Dokumen ini bisa dipakai sebagai peta saat membaca kode dan melakukan
perubahan berikutnya.
