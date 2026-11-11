import { ExternalLink, ShieldCheck } from 'lucide-react';
import { useApp } from '../store';
import {
  APP_RELEASE_DATE,
  APP_VERSION,
  DEVICE_NAME,
  FIRMWARE_RELEASE_DATE,
  REPO_URL,
} from '../version';
import { Badge, Card } from '../components/ui';

function formatTime(value: number | null): string {
  if (!value) return 'belum ada';
  return new Date(value).toLocaleString();
}

export default function Settings() {
  const user = useApp((state) => state.user);
  const snapshot = useApp((state) => state.snapshot);
  const status = snapshot?.status;

  return (
    <div className="space-y-4">
      <Card title="Sesi" subtitle="Hak akses dan koneksi">
        <div className="grid gap-3 sm:grid-cols-2">
          <Info label="User" value={user?.username ?? '--'} />
          <Info label="Role" value={user?.role ?? '--'} />
          <Info label="Nama alat" value={DEVICE_NAME} />
          <Info label="Device ID" value={snapshot?.deviceId ?? 'belum terdeteksi'} />
          <Info label="Broker" value={snapshot?.brokerConnected ? 'terhubung' : 'terputus'} />
          <Info label="Data terakhir" value={formatTime(snapshot?.lastAt ?? null)} />
        </div>
        <div className="mt-4 flex items-start gap-2 rounded-xl border border-safe/20 bg-safe/5 p-3 text-xs text-slate-400">
          <ShieldCheck size={16} className="mt-0.5 shrink-0 text-safe" />
          Kredensial MQTT tidak pernah dikirim ke browser. Semua command melewati server dan
          diverifikasi role di server.
        </div>
      </Card>

      <Card title="Versi dan informasi" subtitle="Dashboard, firmware, dan repositori">
        <div className="grid gap-3 sm:grid-cols-2">
          <Info label="Nama alat" value={DEVICE_NAME} />
          <Info label="Firmware" value={status?.ver ?? 'belum terhubung'} />
          <Info label="Tanggal rilis firmware" value={FIRMWARE_RELEASE_DATE} />
          <Info label="Dashboard web" value={`v${APP_VERSION}`} />
          <Info label="Tanggal rilis dashboard" value={APP_RELEASE_DATE} />
          <Info label="OTA" value={status?.ota_aktif ? 'aktif' : status?.ota_hasil === 1 ? 'OK' : status?.ota_hasil === 2 ? 'ERROR' : 'siap'} />
        </div>

        <div className="mt-4 rounded-xl border border-line bg-ink/30 p-3">
          <div className="text-[10px] uppercase tracking-wide text-slate-500">Repositori GitHub</div>
          <a
            href={REPO_URL}
            target="_blank"
            rel="noreferrer"
            className="mt-1 inline-flex items-center gap-1.5 break-all text-sm font-medium text-sky-300 hover:text-sky-200"
          >
            {REPO_URL}
            <ExternalLink size={13} className="shrink-0" />
          </a>
        </div>

        <div className="mt-3 flex flex-wrap gap-2">
          <Badge tone="info">UI v{APP_VERSION}</Badge>
          <Badge tone="muted">Firmware {status?.ver ?? '--'}</Badge>
          <Badge tone={snapshot?.stale ? 'danger' : 'safe'}>
            {snapshot?.stale ? 'STALE' : 'LIVE'}
          </Badge>
        </div>
      </Card>

      <Card title="Konfigurasi" subtitle="Lokasi pengaturan">
        <p className="text-sm leading-relaxed text-slate-400">
          Konfigurasi preset, hasil kalibrasi, dan isi tangki ada di halaman
          <span className="mx-1 font-medium text-slate-200">Preset &amp; Kalibrasi</span>
          sebelum Settings. Kalibrasi motor/sensor tetap dilakukan dari panel fisik alat.
        </p>
      </Card>
    </div>
  );
}

function Info({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded-xl border border-line bg-ink/30 p-3">
      <div className="text-[10px] uppercase tracking-wide text-slate-500">{label}</div>
      <div className="mt-1 truncate text-sm font-medium text-slate-200">{value}</div>
    </div>
  );
}
