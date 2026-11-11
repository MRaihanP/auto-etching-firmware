import { useEffect, useMemo, useState } from 'react';
import {
  Activity,
  CircleAlert,
  Droplets,
  Pause,
  Play,
  Radio,
  RefreshCw,
  Server,
  ShieldCheck,
  Square,
  Thermometer,
} from 'lucide-react';
import * as api from '../api';
import { isDemoMode } from '../demo';
import { useApp } from '../store';
import { DEVICE_NAME } from '../version';
import type { Status } from '../types';
import { Badge, Button, Card, Dialog, Gauge } from '../components/ui';

const tankLabels = ['HCl', 'H2O2', 'H2O', 'Sabun', 'Limbah'];
const levelLabels = ['HCl', 'H2O2', 'H2O', 'Sabun', 'Limbah'];

function fmt(value: number | undefined, suffix = ''): string {
  return value === undefined ? '--' : `${Math.round(value * 10) / 10}${suffix}`;
}

function lastSeen(value: number | null): string {
  if (!value) return 'belum ada';
  const seconds = Math.max(0, Math.round((Date.now() - value) / 1000));
  return `${seconds}s lalu`;
}

function safetyTone(status: Status | null): 'safe' | 'warn' | 'danger' {
  const state = status?.safety?.state ?? 0;
  if (state === 1) return 'danger';
  if (state === 2) return 'danger';
  return 'safe';
}

export default function Dashboard({ onOpenEvents }: { onOpenEvents: () => void }) {
  const user = useApp((state) => state.user);
  const snapshot = useApp((state) => state.snapshot);
  const events = useApp((state) => state.events);
  const refresh = useApp((state) => state.refresh);
  const setToast = useApp((state) => state.setToast);
  const [pending, setPending] = useState('');
  const [selectedPreset, setSelectedPreset] = useState(0);
  const [confirm, setConfirm] = useState<null | { cmd: string; title: string; text: string }>(null);

  const status = snapshot?.status ?? null;
  const canOperate = user?.role !== 'viewer';
  const controlsDisabled = !canOperate || !snapshot || snapshot.stale || !snapshot.brokerConnected;
  const safety = status?.safety;
  const safetyState = safety?.state ?? 0;
  const cycle = status?.siklus ?? 0;
  const cycleTotal = status?.siklus_total ?? 0;
  const progress = cycleTotal > 0 ? Math.min(100, Math.max(0, ((cycle - 1) / cycleTotal) * 100)) : 0;
  const tanks = status?.tank ?? [];
  const levels = status?.level ?? [];
  const temps = status?.suhu ?? [];
  const tempValid = status?.suhu_valid ?? [];
  const presets = snapshot?.presets ?? [];

  useEffect(() => {
    if (status?.preset !== undefined) setSelectedPreset(status.preset);
  }, [status?.preset]);

  const processLabel = useMemo(() => {
    if (status?.pemulihan) return 'BUTUH PEMULIHAN';
    if (status?.tunggu_pcb) return 'WAIT PCB';
    if (status?.jeda) return 'JEDA';
    if (status?.jalan) return 'BERJALAN';
    return 'IDLE';
  }, [status]);

  const run = async (cmd: string) => {
    if (isDemoMode()) {
      setToast(`Mode demo: command ${cmd.toUpperCase()} tidak dikirim.`);
      setPending('');
      setConfirm(null);
      return;
    }
    setPending(cmd);
    try {
      await api.sendCommand(cmd, cmd === 'start' ? selectedPreset : undefined);
      setToast(`Command ${cmd.toUpperCase()} diterima alat.`);
      await refresh();
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Command gagal');
    } finally {
      setPending('');
      setConfirm(null);
    }
  };

  const requestCommand = (cmd: string) => {
    if (cmd === 'cancel') {
      setConfirm({
        cmd,
        title: 'Batalkan proses?',
        text: 'Semua aktuator berhenti dan pompa limbah mengevakuasi cairan dari nampan. Tindakan ini tidak bisa dibatalkan.',
      });
      return;
    }
    if (cmd === 'drain') {
      setConfirm({
        cmd,
        title: 'Drain nampan?',
        text: 'Pompa limbah akan menyala sampai volume nampan tersedot. Pastikan tangki limbah punya ruang.',
      });
      return;
    }
    if (cmd === 'start') {
      setConfirm({
        cmd,
        title: 'Mulai proses?',
        text: 'Alat akan melakukan precheck lalu berhenti di WAIT_PCB. Konfirmasi PCB bisa dari dashboard atau ENTER di alat.',
      });
      return;
    }
    if (cmd === 'pcb') {
      setConfirm({
        cmd,
        title: 'Konfirmasi PCB sudah di nampan?',
        text: 'Pastikan PCB sudah terpasang dengan benar. Tindakan ini melewati konfirmasi ENTER lokal.',
      });
      return;
    }
    if (cmd === 'clear') {
      setConfirm({
        cmd,
        title: 'Clear latch E-stop?',
        text: 'Pastikan tombol E-stop fisik sudah dilepas. Clear tetap akan ditolak alat jika tombol masih ditekan atau evakuasi cairan belum selesai.',
      });
      return;
    }
    void run(cmd);
  };

  const applyPreset = async () => {
    if (isDemoMode()) {
      setToast(`Mode demo: preset ${selectedPreset + 1} tidak dikirim.`);
      return;
    }
    setPending('preset');
    try {
      await api.sendCommand('preset', selectedPreset);
      setToast(`Preset ${selectedPreset + 1} dipilih.`);
      await refresh();
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Gagal memilih preset');
    } finally {
      setPending('');
    }
  };

  if (!snapshot) {
    return (
      <div className="flex min-h-64 items-center justify-center rounded-2xl border border-line bg-panel text-sm text-slate-400">
        <RefreshCw className="mr-2 animate-spin" size={16} /> Memuat status alat...
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <div className="flex flex-wrap items-center justify-between gap-2">
        <div className="flex items-center gap-2">
          <Badge tone={snapshot.stale || !snapshot.online ? 'danger' : 'safe'}>
            <Radio size={12} /> {snapshot.stale ? 'STALE' : snapshot.online ? 'ONLINE' : 'OFFLINE'}
          </Badge>
          <Badge tone={snapshot.brokerConnected ? 'safe' : 'danger'}>
            <Server size={12} /> {snapshot.brokerConnected ? 'BROKER OK' : 'BROKER OFFLINE'}
          </Badge>
          <Badge tone={safetyTone(status)}>
            <ShieldCheck size={12} /> {safety?.nama ?? 'AMAN'}
          </Badge>
        </div>
        <Button tone="ghost" onClick={() => void refresh()}>
          <RefreshCw size={14} /> Refresh
        </Button>
      </div>

      {safetyState !== 0 && (
        <div className="rounded-2xl border border-danger/50 bg-danger/15 p-4">
          <div className="flex items-start gap-3">
            <CircleAlert className="mt-0.5 text-danger" size={20} />
            <div>
              <div className="text-sm font-semibold text-danger">
                {safety?.nama ?? 'FAULT'} {safety?.kode ? `(E${String(safety.kode).padStart(2, '0')})` : ''}
              </div>
              <div className="mt-1 text-xs text-slate-200">
                {safety?.detail || 'Alat dalam kondisi tidak aman.'}
              </div>
              {safety?.aksi && <div className="mt-1 text-xs text-slate-400">{safety.aksi}</div>}
              <div className="mt-3 flex flex-wrap items-center gap-2">
                {canOperate && safetyState === 1 && safety?.clear_remote ? (
                  <Button tone="danger" onClick={() => requestCommand('clear')} disabled={pending !== ''}>
                    Clear E-stop
                  </Button>
                ) : (
                  <span className="text-[11px] text-slate-400">
                    {safetyState === 1
                      ? 'Lepas tombol E-stop fisik dulu; clear akan aktif setelah pin HIGH dan evakuasi selesai.'
                      : 'Clear error non-E-stop tetap harus dari panel alat.'}
                  </span>
                )}
              </div>
            </div>
          </div>
        </div>
      )}

      <div className="grid gap-4 xl:grid-cols-[1.35fr_1fr]">
        <Card
          title="Proses"
          subtitle={`${status?.preset_nama ?? 'Preset aktif'} · ${processLabel}`}
          action={<Badge tone={status?.jalan ? 'safe' : 'muted'}>{status?.jalan ? 'RUNNING' : 'STOP'}</Badge>}
        >
          <div className="grid gap-3 sm:grid-cols-3">
            <div className="rounded-xl border border-line bg-ink/40 p-3">
              <div className="text-[10px] uppercase tracking-wide text-slate-500">Fase</div>
              <div className="mt-1 text-sm font-semibold text-slate-100">{status?.fase_nama ?? '--'}</div>
              <div className="mt-1 text-[11px] text-slate-400">{status?.langkah_nama ?? '--'}</div>
            </div>
            <div className="rounded-xl border border-line bg-ink/40 p-3">
              <div className="text-[10px] uppercase tracking-wide text-slate-500">Siklus</div>
              <div className="mt-1 text-sm font-semibold text-slate-100">
                {cycle || '--'} / {cycleTotal || '--'}
              </div>
              <div className="mt-2 h-1.5 overflow-hidden rounded-full bg-white/10">
                <div className="h-full rounded-full bg-safe transition-all" style={{ width: `${progress}%` }} />
              </div>
            </div>
            <div className="rounded-xl border border-line bg-ink/40 p-3">
              <div className="text-[10px] uppercase tracking-wide text-slate-500">Nampan</div>
              <div className="mt-1 text-sm font-semibold text-slate-100">{fmt(status?.volume_ml, ' ml')}</div>
              <div className="mt-1 text-[11px] text-slate-400">Dosing: {status?.dosis?.nama ?? '--'}</div>
              <div className="text-[11px] text-slate-500">
                {status?.dosis ? `${status.dosis.sudah ?? 0}/${status.dosis.target ?? 0} ml` : ''}
              </div>
            </div>
          </div>

          <div className="mt-4 flex flex-wrap items-end gap-2 rounded-xl border border-line bg-ink/30 p-3">
            <label className="min-w-48 flex-1 text-[10px] uppercase tracking-wide text-slate-500">
              Preset aktif
              <select
                value={selectedPreset}
                onChange={(event) => setSelectedPreset(Number(event.target.value))}
                disabled={controlsDisabled || Boolean(status?.jalan) || Boolean(status?.jeda)}
                className="mt-1 w-full rounded-lg border border-line bg-panel px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
              >
                {(presets.length ? presets : [{ idx: status?.preset ?? 0, nama: status?.preset_nama ?? 'Preset aktif' }]).map((preset) => (
                  <option key={preset.idx} value={preset.idx}>
                    {preset.idx + 1}. {preset.nama}
                  </option>
                ))}
              </select>
            </label>
            <Button
              tone="secondary"
              disabled={controlsDisabled || Boolean(status?.jalan) || selectedPreset === status?.preset || pending !== ''}
              onClick={() => void applyPreset()}
            >
              Terapkan preset
            </Button>
          </div>

          <div className="mt-3 flex flex-wrap gap-2">
            <Button
              tone="primary"
              disabled={
                controlsDisabled ||
                Boolean(status?.jalan) ||
                Boolean(status?.jeda) ||
                Boolean(status?.tunggu_pcb) ||
                Boolean(status?.pemulihan)
              }
              onClick={() => requestCommand('start')}
            >
              <Play size={15} /> {pending === 'start' ? 'Memulai...' : 'Mulai'}
            </Button>
            {status?.jeda ? (
              <Button
                tone="warn"
                disabled={controlsDisabled || pending !== ''}
                onClick={() => void run('resume')}
              >
                <Play size={15} /> Lanjutkan
              </Button>
            ) : (
              <Button
                tone="secondary"
                disabled={controlsDisabled || !status?.jalan || pending !== ''}
                onClick={() => void run('pause')}
              >
                <Pause size={15} /> Jeda
              </Button>
            )}
            <Button
              tone="danger"
              disabled={controlsDisabled || (!status?.jalan && !status?.jeda && !status?.tunggu_pcb)}
              onClick={() => requestCommand('cancel')}
            >
              <Square size={15} /> Batalkan
            </Button>
            <Button
              tone="secondary"
              disabled={controlsDisabled || Boolean(status?.jalan) || pending !== ''}
              onClick={() => requestCommand('drain')}
            >
              <Droplets size={15} /> Drain
            </Button>
            {status?.tunggu_pcb && (
              <Button
                tone="warn"
                disabled={controlsDisabled || pending !== ''}
                onClick={() => requestCommand('pcb')}
              >
                PCB sudah di nampan
              </Button>
            )}
          </div>

          {status?.tunggu_pcb && (
            <p className="mt-3 rounded-lg border border-warn/30 bg-warn/10 px-3 py-2 text-xs text-warn">
              Pasang PCB, lalu konfirmasi dari dashboard atau tekan ENTER di alat.
            </p>
          )}
          {user?.role === 'viewer' && (
            <p className="mt-3 text-xs text-slate-500">Role viewer hanya dapat memantau.</p>
          )}
        </Card>

        <Card title={DEVICE_NAME} subtitle={snapshot.deviceId ?? 'device belum terdeteksi'}>
          <div className="grid gap-2 text-xs">
            <Row label="Broker" value={snapshot.brokerConnected ? 'terhubung' : 'terputus'} />
            <Row label="Alat" value={snapshot.online ? 'online' : 'offline'} />
            <Row label="Status data" value={snapshot.stale ? 'stale' : 'segar'} />
            <Row label="Terakhir" value={lastSeen(snapshot.lastAt)} />
            <Row label="Firmware" value={status?.ver ?? '--'} />
            <Row label="Heap" value={status?.heap ? `${Math.round(status.heap / 1024)} KB` : '--'} />
            <Row label="OTA" value={status?.ota_aktif ? 'aktif' : status?.ota_hasil === 1 ? 'OK' : status?.ota_hasil === 2 ? 'ERROR' : 'siap'} />
          </div>
        </Card>
      </div>

      <div className="grid gap-4 xl:grid-cols-2">
        <Card title="Tangki sumber dan limbah" subtitle="ml tersisa / kapasitas">
          <div className="grid gap-2 sm:grid-cols-2">
            {tankLabels.map((label, index) => {
              const item = tanks[index] ?? {};
              const value = Number(item.ml ?? 0);
              const max = Number(item.kap ?? 1);
              const tone = index === 4 && value / max > 0.85 ? 'danger' : value / max < 0.15 ? 'warn' : 'safe';
              return <Gauge key={label} label={label} value={value} max={max} tone={tone} />;
            })}
          </div>
        </Card>

        <Card title="Sensor" subtitle="Suhu, level, dan kalibrasi">
          <div className="grid gap-3 sm:grid-cols-2">
            <div className="rounded-xl border border-line bg-ink/40 p-3">
              <div className="mb-2 flex items-center gap-2 text-xs font-medium text-slate-300">
                <Thermometer size={14} className="text-warn" /> Suhu
              </div>
              <div className="space-y-1 text-xs text-slate-400">
                {['Motor', 'Komponen', 'Chamber'].map((name, index) => (
                  <div key={name} className="flex justify-between">
                    <span>{name}</span>
                    <span className={tempValid[index] === false ? 'text-danger' : 'text-slate-200'}>
                      {fmt(temps[index], ' C')}
                    </span>
                  </div>
                ))}
              </div>
            </div>
            <div className="rounded-xl border border-line bg-ink/40 p-3">
              <div className="mb-2 flex items-center gap-2 text-xs font-medium text-slate-300">
                <Activity size={14} className="text-safe" /> Level
              </div>
              <div className="space-y-1 text-xs text-slate-400">
                {levelLabels.map((name, index) => {
                  const item = levels[index] ?? {};
                  return (
                    <div key={name} className="flex justify-between">
                      <span>{name}</span>
                      <span className={item.fault ? 'text-danger' : 'text-slate-200'}>
                        {item.fault ? 'FAULT' : `${item.p ?? 0}%`}
                      </span>
                    </div>
                  );
                })}
              </div>
            </div>
          </div>
        </Card>
      </div>

      <Card
        title="Event terakhir"
        subtitle="Kejadian terbaru dari alat"
        action={
          <Button tone="ghost" onClick={onOpenEvents}>
            Semua event
          </Button>
        }
      >
        {events.length === 0 ? (
          <p className="text-xs text-slate-500">Belum ada event.</p>
        ) : (
          <div className="space-y-2">
            {events.slice(0, 6).map((event, index) => (
              <div key={`${event.t}-${event.kode}-${index}`} className="flex gap-3 rounded-lg border border-line bg-ink/30 px-3 py-2">
                <Badge tone={event.kode >= 1 && event.kode <= 10 ? 'danger' : 'info'}>
                  {event.kode.toString().padStart(3, '0')}
                </Badge>
                <div className="min-w-0 flex-1">
                  <div className="truncate text-xs text-slate-200">{event.detail || '-'}</div>
                  <div className="text-[10px] text-slate-500">
                    {event.epoch > 0 ? new Date(event.epoch * 1000).toLocaleString() : `${event.t} ms`}
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </Card>

      <Dialog
        open={Boolean(confirm)}
        title={confirm?.title ?? ''}
        confirmLabel={confirm?.cmd === 'start' ? 'Mulai' : 'Lanjutkan'}
        confirmTone={confirm?.cmd === 'cancel' ? 'danger' : 'warn'}
        busy={pending !== ''}
        onClose={() => setConfirm(null)}
        onConfirm={() => confirm && void run(confirm.cmd)}
      >
        {confirm?.text}
      </Dialog>
    </div>
  );
}

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-center justify-between gap-3 border-b border-line/50 py-1.5 last:border-0">
      <span className="text-slate-500">{label}</span>
      <span className="truncate text-right font-medium text-slate-200">{value}</span>
    </div>
  );
}
