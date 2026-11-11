# Etching Dashboard (self-hosted)

Webapp monitoring dan kontrol untuk firmware `auto-etching-firmware` v1.2.0.
Browser **tidak** terhubung ke MQTT; semua akses broker lewat server dashboard
yang berjalan sendiri.

Panduan arsitektur, deployment 24/7, keamanan, dan jalur belajar web
development ada di **[ARSITEKTUR-DAN-PROSES.md](./ARSITEKTUR-DAN-PROSES.md)**.

```text
Browser --HTTPS/WebSocket--> Dashboard server --MQTT TLS 8883--> Broker --> Alat
```

Tidak ada layanan pihak ketiga: auth, session, audit, database, dan static
asset semuanya lokal.

## Fitur

- Login lokal dengan role `viewer`, `operator`, `admin`.
- Status real-time: fase, siklus, dosing, tank, suhu, level, safety, event.
- Command: start, pause, resume, cancel, drain, status, pilih preset, konfirmasi PCB.
- Halaman **Preset & Kalibrasi** sebelum Settings: editor preset 19 field,
  hasil kalibrasi semua motor/sensor, lalu isi tangki.
- Pilih preset sebelum start; perubahan preset tersimpan ke NVS alat.
- Clear E-stop dari dashboard hanya setelah tombol fisik dilepas dan evakuasi selesai.
- Hasil kalibrasi stepper, DC, agitasi, level, suhu, MQ-135 + tanggal terakhir.
- Config tank/setting (admin, IDLE-only); Settings berisi versi/repo/info.
- ACK correlation, timeout, rate limit, audit.
- Status stale/offline; kontrol dinonaktifkan saat data tidak valid.
- WebSocket push; PWA installable; layout desktop/mobile; panel kiri bisa minimize.
- Kredensial broker hanya di server (`server/.env`), tidak pernah di frontend.

## Prasyarat

- Node.js 22+ dan pnpm.
- Broker MQTT yang bisa diakses server lewat TLS 8883.
- User broker khusus dashboard dengan ACL:
  - subscribe `pelarut/+/status`, `telemetry`, `event`, `ack`, `online`;
  - publish `pelarut/<deviceId>/cmd`;
  - bukan admin broker.

## Setup Lokal

```bash
cd webapp
pnpm install
cp server/.env.example server/.env
# edit server/.env: MQTT_USER, MQTT_PASS, ADMIN_PASSWORD, SESSION_SECRET
pnpm seed
pnpm build
pnpm start
```

Buka `http://localhost:8080`. Admin pertama dan password dibuat oleh
`pnpm seed` dari nilai di `server/.env`. File `server/.env` di-ignore Git.

Mode development (server + Vite):

```bash
pnpm dev
# buka http://localhost:5173
```

## Preview UI Saja (tanpa MQTT/server)

Mode demo memakai data contoh, tidak membuka API/WebSocket, dan tidak
mengirim command ke alat:

```bash
cd webapp
pnpm install
pnpm --dir web dev
# buka http://localhost:5173/?demo=1
```

Untuk build produksi:

```bash
pnpm --dir web build
pnpm --dir web preview
# buka http://localhost:4173/?demo=1
```

`?demo=1` hanya untuk melihat layout/komponen. Jangan gunakan untuk
mengontrol alat; gunakan dashboard normal tanpa parameter demo.

## Konfigurasi Penting

| Variabel | Arti |
|---|---|
| `HOST`, `PORT` | Listen server |
| `PUBLIC_ORIGIN` | Origin dashboard untuk log/dokumentasi |
| `COOKIE_SECURE` | `true` di produksi HTTPS |
| `TRUST_PROXY` | `true` jika di belakang Caddy/Nginx |
| `MQTT_URL` | `mqtts://mqtt.loggerindonesia.com:8883` |
| `MQTT_USER`, `MQTT_PASS` | Kredensial server (jangan di frontend) |
| `MQTT_BASE` | Default `pelarut` |
| `DEVICE_ID` | Kosongkan untuk auto-detect dari topik status |
| `MQTT_CA_PATH` | Opsional CA PEM bila sistem tidak memercayai chain broker |
| `DB_PATH` | SQLite lokal |
| `ADMIN_USER`, `ADMIN_PASSWORD` | Dipakai `pnpm seed` |

Device ID diambil otomatis dari `pelarut/+/status` dan ditampilkan di UI.
Untuk mengunci satu alat, isi `DEVICE_ID` (mis. `esp32-2C01FC`).

## Keamanan

- Browser tidak memiliki URL broker, username, password, atau topik mentah.
- Session cookie `HttpOnly; Secure; SameSite=Strict` saat HTTPS.
- Password user di-hash dengan scrypt (Node built-in).
- CSRF token wajib untuk semua POST.
- Rate limit login dan command.
- Role diperiksa di server, bukan hanya UI.
- `clear` E-stop tersedia hanya jika firmware melaporkan `clear_remote=true`;
  error non-E-stop tetap lokal.
- `start` tetap berhenti di `WAIT_PCB`; konfirmasi PCB lewat tombol dashboard
  atau ENTER lokal.
- Audit command disimpan di SQLite.
- Jangan commit `server/.env`, `server/data/`, atau build output.

## Deployment Self-Hosted

1. Salin repo/aplikasi ke server.
2. Buat user sistem dan direktori data.
3. Simpan env di `/etc/etching-dashboard.env` permission `600`.
4. Build:

   ```bash
   cd webapp
   pnpm install --prod=false
   pnpm build
   pnpm seed
   ```

5. Jalankan service:

   ```bash
   sudo systemctl enable --now etching-dashboard
   ```

Contoh unit dan reverse proxy ada di `deploy/`.

### TLS

- LAN: self-signed/private CA; install CA di perangkat client.
- Domain: certificate yang dimiliki pengguna, dipasang di Caddy/Nginx.
- Jangan memakai HTTP untuk akses internet.

### Backup

Backup `DB_PATH` (SQLite) secara berkala. Jangan backup file `.env` ke
tempat publik. Rotasi password admin/broker jika ada indikasi kebocoran.

## API Ringkas

| Method | Path | Role |
|---|---|---|
| POST | `/api/auth/login` | publik |
| POST | `/api/auth/logout` | login |
| GET | `/api/me` | login |
| GET | `/api/status` | login |
| GET | `/api/events` | login |
| GET | `/api/audit` | admin |
| POST | `/api/commands` | operator/admin |
| POST | `/api/config` | admin |
| GET | `/api/health` | publik |
| GET | `/ws` | login (cookie) |

Command body:

```json
{ "cmd": "pause" }
{ "cmd": "start", "preset": 0 }
{ "cmd": "preset", "preset": 0 }
{ "cmd": "pcb" }
{ "cmd": "clear" }
```

`pcb` hanya saat alat `WAIT_PCB`. `clear` hanya saat safety state E-stop dan
status alat melaporkan `clear_remote: true` (tombol fisik sudah HIGH dan
evakuasi selesai). Error non-E-stop tetap harus clear lokal.

Config body:

```json
{"target":"tank","idx":0,"ml":900}
{"target":"setting","name":"drainMarginPct","value":25}
{"target":"preset","idx":0,"fields":{"siklus":2,"agitasiDetik":300}}
```

## Batasan

- Dashboard bukan pengganti E-stop fisik.
- Config hanya saat alat IDLE dan safety AMAN.
- `clear` harus dari panel alat.
- `start` tidak bisa melewati `WAIT_PCB`.
- Jika broker tidak mendukung ACL khusus dashboard, jangan buka dashboard ke
  internet.
