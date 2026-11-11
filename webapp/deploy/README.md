# Deploy Self-Hosted

Contoh ini mengasumsikan aplikasi di `/opt/etching-webapp`, user sistem
`etching`, dan database di `/var/lib/etching-dashboard`.

## 1. Siapkan direktori

```bash
sudo useradd --system --home /opt/etching-webapp --shell /usr/sbin/nologin etching
sudo mkdir -p /opt/etching-webapp /var/lib/etching-dashboard /etc
sudo chown -R etching:etching /opt/etching-webapp /var/lib/etching-dashboard
```

## 2. Environment

Buat `/etc/etching-dashboard.env` dengan permission `600`:

```bash
sudo install -m 600 -o root -g root /dev/null /etc/etching-dashboard.env
sudo editor /etc/etching-dashboard.env
```

Isi minimal:

```text
HOST=127.0.0.1
PORT=8080
PUBLIC_ORIGIN=https://dashboard.example.com
COOKIE_SECURE=true
TRUST_PROXY=true
DB_PATH=/var/lib/etching-dashboard/dashboard.db
WEB_DIST=/opt/etching-webapp/web/dist

MQTT_URL=mqtts://mqtt.loggerindonesia.com:8883
MQTT_USER=CHANGE_ME
MQTT_PASS=CHANGE_ME
MQTT_BASE=pelarut
DEVICE_ID=
MQTT_CA_PATH=

ADMIN_USER=admin
ADMIN_PASSWORD=CHANGE_ME_LONG_RANDOM
```

`ADMIN_PASSWORD` hanya perlu ada saat menjalankan seed; setelah itu bisa
dihapus dari env produksi jika tidak diperlukan.

## 3. Build dan seed

```bash
cd /opt/etching-webapp
sudo -u etching pnpm install --prod=false
sudo -u etching pnpm build
# seed memakai env yang sama
sudo -u etching env $(grep -v '^#' /etc/etching-dashboard.env | xargs) pnpm seed
```

## 4. Service

```bash
sudo cp deploy/etching-dashboard.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now etching-dashboard
sudo systemctl status etching-dashboard
```

## 5. Reverse proxy

Gunakan `Caddyfile.example` atau konfigurasi Nginx. Pastikan hanya port
reverse proxy (443) yang terbuka; port MQTT tidak diekspos ke browser.

## 6. Update

```bash
sudo systemctl stop etching-dashboard
sudo -u etching pnpm install --prod=false
sudo -u etching pnpm build
sudo systemctl start etching-dashboard
```

Backup database sebelum update.
