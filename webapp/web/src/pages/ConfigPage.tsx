import { useEffect, useState } from 'react';
import { SlidersHorizontal } from 'lucide-react';
import * as api from '../api';
import { isDemoMode } from '../demo';
import { useApp } from '../store';
import { CalibrationCard } from '../components/CalibrationCard';
import { PresetEditor } from '../components/PresetEditor';
import { Button, Card, Dialog, Gauge } from '../components/ui';

const tankLabels = ['HCl', 'H2O2', 'H2O', 'Sabun', 'Limbah'];

export default function ConfigPage() {
  const user = useApp((state) => state.user);
  const snapshot = useApp((state) => state.snapshot);
  const refresh = useApp((state) => state.refresh);
  const setToast = useApp((state) => state.setToast);
  const [tanks, setTanks] = useState<string[]>(['', '', '', '', '']);
  const [json, setJson] = useState('{"target":"setting","name":"drainMarginPct","value":25}');
  const [busy, setBusy] = useState(false);
  const [confirm, setConfirm] = useState<null | {
    payloads: Record<string, unknown>[];
    text: string;
  }>(null);
  const admin = user?.role === 'admin';
  const tankData = snapshot?.status?.tank ?? [];

  useEffect(() => {
    if (tanks.some(Boolean)) return;
    if (tankData.length) setTanks(tankData.map((item) => String(item.ml ?? 0)));
    // Isi sekali saja; jangan menimpa input yang sedang diedit.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [snapshot?.deviceId]);

  const send = async () => {
    if (!confirm) return;
    if (isDemoMode()) {
      setToast('Mode demo: config tidak dikirim ke alat.');
      setConfirm(null);
      return;
    }
    setBusy(true);
    try {
      for (const payload of confirm.payloads) await api.sendConfig(payload);
      setToast('Config diterima alat.');
      setConfirm(null);
      await refresh();
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Config gagal');
    } finally {
      setBusy(false);
    }
  };

  const submitTank = () => {
    const values = tanks.map((value) => Number(value));
    if (values.some((value) => !Number.isFinite(value) || value < 0)) {
      setToast('Volume tangki harus angka >= 0');
      return;
    }
    setConfirm({
      payloads: values.map((ml, idx) => ({ target: 'tank', idx, ml: Math.round(ml) })),
      text: `Kirim isi 5 tangki (${values.map((value) => Math.round(value)).join(' / ')} ml)?`,
    });
  };

  const submitJson = () => {
    try {
      const payload = JSON.parse(json) as Record<string, unknown>;
      if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
        throw new Error('JSON harus object');
      }
      setConfirm({
        payloads: [payload],
        text: 'Kirim config ini ke alat? Pastikan alat IDLE dan safety AMAN.',
      });
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'JSON tidak valid');
    }
  };

  return (
    <div className="space-y-4">
      <Card
        title="Konfigurasi preset"
        subtitle="19 field; tersimpan ke NVS alat dan hanya saat IDLE"
        action={<SlidersHorizontal size={17} className="text-safe" />}
      >
        {admin ? (
          <PresetEditor
            presets={snapshot?.presets ?? []}
            onSave={(payload) =>
              setConfirm({
                payloads: [payload],
                text: 'Simpan perubahan preset ini ke alat? Pastikan alat IDLE dan safety AMAN.',
              })
            }
          />
        ) : (
          <p className="text-sm text-slate-500">
            Hanya admin yang dapat mengubah preset. Hasil kalibrasi tetap dapat dilihat.
          </p>
        )}
      </Card>

      <CalibrationCard status={snapshot?.status ?? null} />

      <Card title="Isi tangki" subtitle="Ditampilkan di bawah hasil kalibrasi level">
        <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
          {tankLabels.map((label, index) => {
            const item = tankData[index] ?? {};
            const value = Number(item.ml ?? 0);
            const max = Number(item.kap ?? 1);
            const tone = index === 4 && value / max > 0.85 ? 'danger' : value / max < 0.15 ? 'warn' : 'safe';
            return <Gauge key={label} label={label} value={value} max={max} tone={tone} />;
          })}
        </div>

        {admin && (
          <>
            <div className="mt-4 grid gap-3 sm:grid-cols-2 lg:grid-cols-5">
              {tankLabels.map((label, index) => (
                <label key={label} className="text-xs text-slate-400">
                  {label} (ml)
                  <input
                    type="number"
                    min={0}
                    value={tanks[index] ?? ''}
                    onChange={(event) => {
                      const next = [...tanks];
                      next[index] = event.target.value;
                      setTanks(next);
                    }}
                    className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
                  />
                </label>
              ))}
            </div>
            <Button className="mt-3" tone="primary" onClick={submitTank} disabled={busy}>
              Simpan isi tangki
            </Button>
          </>
        )}
      </Card>

      {admin && (
        <Card title="Advanced JSON" subtitle="Setting atau preset">
          <textarea
            value={json}
            onChange={(event) => setJson(event.target.value)}
            rows={6}
            spellCheck={false}
            className="w-full rounded-xl border border-line bg-ink px-3 py-2 font-mono text-xs text-slate-200 outline-none focus:border-safe"
          />
          <Button className="mt-3" tone="warn" onClick={submitJson} disabled={busy}>
            Kirim config JSON
          </Button>
          <pre className="mt-3 overflow-x-auto rounded-lg border border-line bg-ink/60 p-3 text-[11px] text-slate-400">{`Tank:
{"target":"tank","idx":0,"ml":900}

Setting:
{"target":"setting","name":"drainMarginPct","value":25}

Preset:
{"target":"preset","idx":0,"fields":{"siklus":2,"agitasiDetik":300}}`}</pre>
        </Card>
      )}

      <Dialog
        open={Boolean(confirm)}
        title="Konfirmasi config"
        confirmLabel="Kirim"
        confirmTone="warn"
        busy={busy}
        onClose={() => setConfirm(null)}
        onConfirm={() => void send()}
      >
        {confirm?.text}
      </Dialog>
    </div>
  );
}
