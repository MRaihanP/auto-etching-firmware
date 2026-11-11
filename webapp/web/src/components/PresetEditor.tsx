import { useEffect, useState } from 'react';
import type { Preset } from '../types';
import { Button } from './ui';

const draftFields: Array<[string, string]> = [
  ['pcbPanjang', 'PCB panjang (mm)'],
  ['pcbLebar', 'PCB lebar (mm)'],
  ['persenHCl', 'HCl (%)'],
  ['persenH2O2', 'H2O2 (%)'],
  ['mlHCl', 'HCl (ml)'],
  ['mlH2O2', 'H2O2 (ml)'],
  ['volume', 'Volume total (ml)'],
  ['isiTembagaPct', 'Isi tembaga (%)'],
  ['tebalTembagaUm', 'Tebal Cu (um)'],
  ['lajuEtchX10', 'Laju etch (0,1 um/mnt)'],
  ['agitasiDetik', 'Etching (detik)'],
  ['agitasiSpeed', 'Speed agitasi (1-3)'],
  ['siklus', 'Siklus'],
  ['bilasSabun', 'Bilas sabun'],
  ['bilasAir', 'Bilas air'],
  ['bilasMl', 'Bilas ml'],
  ['bilasDetik', 'Bilas detik'],
];

function presetToDraft(preset: Preset): Record<string, string> {
  const out: Record<string, string> = { nama: preset.nama };
  for (const [key] of draftFields) {
    out[key] = String(preset[key as keyof Preset] ?? 0);
  }
  out.modeRasio = String(preset.modeRasio ?? 0);
  out.modeWaktu = String(preset.modeWaktu ?? 0);
  out.konfirmasiSiklus = String(preset.konfirmasiSiklus ?? 0);
  return out;
}

export function PresetEditor({
  presets,
  onSave,
}: {
  presets: Preset[];
  onSave: (payload: Record<string, unknown>) => void;
}) {
  const [index, setIndex] = useState(0);
  const [draft, setDraft] = useState<Record<string, string>>({});

  useEffect(() => {
    const preset = presets[index];
    if (preset) setDraft(presetToDraft(preset));
  }, [index, presets.length]);

  if (!presets.length) {
    return (
      <p className="text-sm text-slate-500">
        Menunggu data preset dari alat. Pastikan MQTT terhubung dan firmware sudah
        mempublikasikan topik <code>presets</code>.
      </p>
    );
  }

  const set = (key: string, value: string) =>
    setDraft((old) => ({ ...old, [key]: value }));

  const save = () => {
    const fields: Record<string, unknown> = { nama: draft.nama ?? '' };
    for (const [key] of draftFields) fields[key] = Number(draft[key] ?? 0);
    fields.modeRasio = Number(draft.modeRasio ?? 0);
    fields.modeWaktu = Number(draft.modeWaktu ?? 0);
    fields.konfirmasiSiklus = Number(draft.konfirmasiSiklus ?? 0);
    onSave({ target: 'preset', idx: index, fields });
  };

  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-end gap-2">
        <label className="min-w-48 flex-1 text-xs text-slate-400">
          Preset
          <select
            value={index}
            onChange={(event) => setIndex(Number(event.target.value))}
            className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
          >
            {presets.map((preset) => (
              <option key={preset.idx} value={preset.idx}>
                {preset.idx + 1}. {preset.nama}
              </option>
            ))}
          </select>
        </label>
        <Button tone="primary" onClick={save}>
          Simpan preset ke alat
        </Button>
      </div>

      <label className="block text-xs text-slate-400">
        Nama
        <input
          value={draft.nama ?? ''}
          onChange={(event) => set('nama', event.target.value)}
          className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
        />
      </label>

      <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
        <label className="text-xs text-slate-400">
          Mode rasio
          <select
            value={draft.modeRasio ?? '0'}
            onChange={(event) => set('modeRasio', event.target.value)}
            className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
          >
            <option value="0">Persen</option>
            <option value="1">ml</option>
          </select>
        </label>
        <label className="text-xs text-slate-400">
          Mode waktu
          <select
            value={draft.modeWaktu ?? '0'}
            onChange={(event) => set('modeWaktu', event.target.value)}
            className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
          >
            <option value="0">Manual</option>
            <option value="1">Otomatis dari PCB</option>
          </select>
        </label>
        <label className="text-xs text-slate-400">
          Konfirmasi siklus
          <select
            value={draft.konfirmasiSiklus ?? '0'}
            onChange={(event) => set('konfirmasiSiklus', event.target.value)}
            className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
          >
            <option value="0">OFF</option>
            <option value="1">ON</option>
          </select>
        </label>
        {draftFields.map(([key, label]) => (
          <label key={key} className="text-xs text-slate-400">
            {label}
            <input
              type="number"
              value={draft[key] ?? ''}
              onChange={(event) => set(key, event.target.value)}
              className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2 text-sm text-slate-100 outline-none focus:border-safe"
            />
          </label>
        ))}
      </div>
      <p className="text-[11px] text-slate-500">
        Tebal Cu dalam um (35 um = 1 oz). Laju etch 30 = 3,0 um/menit. Jika mode
        rasio persen, firmware menghitung ulang ml HCl/H2O2/H2O dari volume.
      </p>
    </div>
  );
}
