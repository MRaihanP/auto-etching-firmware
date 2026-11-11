import { useRef, useState, type DragEvent } from 'react';
import { ExternalLink, FileUp, ShieldCheck } from 'lucide-react';
import * as api from '../api';
import { useApp } from '../store';
import {
  APP_RELEASE_DATE,
  APP_VERSION,
  DEVICE_NAME,
  FIRMWARE_RELEASE_DATE,
  REPO_URL,
} from '../version';
import { Badge, Button, Card } from '../components/ui';

function formatTime(value: number | null): string {
  if (!value) return 'belum ada';
  return new Date(value).toLocaleString();
}

export default function Settings() {
  const user = useApp((state) => state.user);
  const snapshot = useApp((state) => state.snapshot);
  const refresh = useApp((state) => state.refresh);
  const setToast = useApp((state) => state.setToast);
  const status = snapshot?.status;
  const [otaFile, setOtaFile] = useState<File | null>(null);
  const [otaBusy, setOtaBusy] = useState(false);
  const [otaDrag, setOtaDrag] = useState(false);
  const fileRef = useRef<HTMLInputElement>(null);
  const admin = user?.role === 'admin';
  const canOta = Boolean(
    admin &&
      snapshot &&
      snapshot.online &&
      !snapshot.stale &&
      snapshot.brokerConnected &&
      !status?.jalan &&
      !status?.jeda &&
      !status?.tunggu_pcb &&
      !status?.pemulihan &&
      (status?.safety?.state ?? 0) === 0,
  );

  const pickFile = (file: File | undefined) => {
    if (!file) return;
    if (!file.name.toLowerCase().endsWith('.bin')) {
      setToast('Pilih file firmware .bin hasil compile Arduino.');
      return;
    }
    setOtaFile(file);
  };

  const onDrop = (event: DragEvent<HTMLDivElement>) => {
    event.preventDefault();
    setOtaDrag(false);
    pickFile(event.dataTransfer.files?.[0]);
  };

  const uploadOta = async () => {
    if (!otaFile) return;
    if (!window.confirm(`Upload ${otaFile.name} ke alat? Alat akan reboot setelah OTA.`)) return;
    setOtaBusy(true);
    try {
      const result = await api.uploadOta(otaFile);
      setToast(`OTA diterima (${Math.round(result.bytes / 1024)} KB). Alat reboot; versi akan diperbarui.`);
      setOtaFile(null);
      window.setTimeout(() => void refresh(), 8000);
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'OTA gagal');
    } finally {
      setOtaBusy(false);
    }
  };

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

      {admin && (
        <Card title="OTA Firmware" subtitle="Upload file .bin hasil compile Arduino; hanya saat IDLE dan safety AMAN">
          <input
            ref={fileRef}
            type="file"
            accept=".bin,application/octet-stream"
            className="hidden"
            onChange={(event) => pickFile(event.target.files?.[0])}
          />
          <div
            onDragOver={(event) => {
              event.preventDefault();
              setOtaDrag(true);
            }}
            onDragLeave={() => setOtaDrag(false)}
            onDrop={onDrop}
            className={`rounded-2xl border-2 border-dashed p-5 text-center transition ${
              otaDrag ? 'border-safe bg-safe/10' : 'border-line bg-ink/30'
            } ${canOta ? '' : 'opacity-60'}`}
          >
            <FileUp size={24} className="mx-auto text-safe" />
            <p className="mt-2 text-sm font-medium text-slate-200">
              Tarik file <code>.bin</code> ke sini
            </p>
            <p className="mt-1 text-xs text-slate-500">atau pilih file dari komputer</p>
            <Button
              tone="secondary"
              className="mt-3"
              onClick={() => fileRef.current?.click()}
              disabled={otaBusy || !canOta}
            >
              Pilih file .bin
            </Button>
            {otaFile && (
              <p className="mt-2 text-xs text-slate-300">
                {otaFile.name} · {Math.round(otaFile.size / 1024)} KB
              </p>
            )}
          </div>
          <div className="mt-3 flex flex-wrap items-center gap-2">
            <Button tone="warn" onClick={() => void uploadOta()} disabled={!otaFile || otaBusy || !canOta}>
              {otaBusy ? 'Mengunggah...' : 'Upload OTA'}
            </Button>
            {otaFile && !otaBusy && (
              <Button tone="ghost" onClick={() => setOtaFile(null)}>
                Hapus pilihan
              </Button>
            )}
            {!canOta && (
              <span className="text-[11px] text-warn">
                OTA hanya aktif saat alat online, IDLE, dan safety AMAN.
              </span>
            )}
          </div>
          <div className="mt-3 grid gap-2 text-xs sm:grid-cols-3">
            <Info label="Firmware aktif" value={status?.ver ?? '--'} />
            <Info label="Build firmware" value={status?.build ?? '--'} />
            <Info label="IP alat" value={status?.ip ?? '--'} />
          </div>
          <p className="mt-2 text-[11px] text-slate-500">
            Setelah upload diterima, alat reboot. Versi di Settings akan berubah otomatis setelah
            status MQTT baru diterima.
          </p>
        </Card>
      )}

      <Card title="Versi dan informasi" subtitle="Dashboard, firmware, dan repositori">
        <div className="grid gap-3 sm:grid-cols-2">
          <Info label="Nama alat" value={DEVICE_NAME} />
          <Info label="Firmware" value={status?.ver ?? 'belum terhubung'} />
          <Info label="Build firmware" value={status?.build ?? '--'} />
          <Info label="Tanggal rilis firmware" value={FIRMWARE_RELEASE_DATE} />
          <Info label="Dashboard web" value={`v${APP_VERSION}`} />
          <Info label="Tanggal rilis dashboard" value={APP_RELEASE_DATE} />
          <Info label="IP alat" value={status?.ip ?? '--'} />
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
