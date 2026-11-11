import { Clock3 } from 'lucide-react';
import type { Status } from '../types';
import { Badge, Card } from './ui';

const stepperNames = ['HCl', 'H2O2', 'Air'];
const dcNames = ['Sabun', 'Limbah'];
const levelNames = ['HCl', 'H2O2', 'H2O', 'Sabun', 'Limbah'];
const tempNames = ['Motor', 'Komponen', 'Chamber'];

function formatEpoch(epoch: number | undefined): string {
  if (!epoch) return 'belum ada';
  return new Date(epoch * 1000).toLocaleString();
}

export function CalibrationCard({ status }: { status: Status | null }) {
  const kalib = status?.kalib;
  const mlPerRev = kalib?.ml_per_rev ?? [];
  const rpm = kalib?.rpm ?? [];
  const stepEpoch = kalib?.stepper_epoch ?? [];
  const dcRate = kalib?.dc ?? [];
  const dcDuty = kalib?.dc_duty ?? [];
  const dcEpoch = kalib?.dc_epoch ?? [];
  const level = kalib?.level ?? [];
  const suhu = kalib?.suhu ?? [];
  const agitasiDuty = kalib?.agitasi_duty ?? [];

  return (
    <Card title="Hasil kalibrasi" subtitle="Tampilan saja; kalibrasi tetap dari panel fisik">
      <div className="grid gap-3 xl:grid-cols-2">
        <div className="rounded-xl border border-line bg-ink/30 p-3">
          <div className="mb-2 text-xs font-medium text-slate-300">Pompa stepper</div>
          <div className="space-y-2">
            {stepperNames.map((name, index) => (
              <div key={name} className="rounded-lg border border-line/60 px-3 py-2 text-xs">
                <div className="flex items-center justify-between gap-2">
                  <span className="font-medium text-slate-200">{name}</span>
                  <Badge tone={kalib?.stepper?.[index] ? 'safe' : 'muted'}>
                    {kalib?.stepper?.[index] ? 'OK' : 'belum'}
                  </Badge>
                </div>
                <div className="mt-1 grid grid-cols-2 gap-x-3 text-slate-400">
                  <span>ml/rev: {mlPerRev[index] ? Number(mlPerRev[index]).toFixed(3) : '--'}</span>
                  <span>RPM: {rpm[index] ?? '--'}</span>
                </div>
                <div className="mt-1 flex items-center gap-1 text-[10px] text-slate-500">
                  <Clock3 size={11} /> {formatEpoch(stepEpoch[index])}
                </div>
              </div>
            ))}
          </div>
        </div>

        <div className="rounded-xl border border-line bg-ink/30 p-3">
          <div className="mb-2 text-xs font-medium text-slate-300">Pompa DC</div>
          <div className="space-y-2">
            {dcNames.map((name, index) => (
              <div key={name} className="rounded-lg border border-line/60 px-3 py-2 text-xs">
                <div className="flex items-center justify-between gap-2">
                  <span className="font-medium text-slate-200">{name}</span>
                  <Badge tone={dcRate[index] > 0 ? 'safe' : 'muted'}>
                    {dcRate[index] > 0 ? 'OK' : 'belum'}
                  </Badge>
                </div>
                <div className="mt-1 grid grid-cols-2 gap-x-3 text-slate-400">
                  <span>laju: {dcRate[index] ? Number(dcRate[index]).toFixed(2) : '--'} ml/s</span>
                  <span>duty: {dcDuty[index] ?? '--'}%</span>
                </div>
                <div className="mt-1 flex items-center gap-1 text-[10px] text-slate-500">
                  <Clock3 size={11} /> {formatEpoch(dcEpoch[index])}
                </div>
              </div>
            ))}
          </div>
          <div className="mt-2 rounded-lg border border-line/60 px-3 py-2 text-xs">
            <div className="flex items-center justify-between gap-2">
              <span className="font-medium text-slate-200">Agitasi</span>
              <Badge tone={agitasiDuty.length ? 'safe' : 'muted'}>
                {agitasiDuty.length ? 'OK' : 'belum'}
              </Badge>
            </div>
            <div className="mt-1 text-slate-400">
              SPD1/2/3: {agitasiDuty.length ? agitasiDuty.map((v) => `${v}%`).join(' / ') : '--'}
            </div>
            <div className="mt-1 flex items-center gap-1 text-[10px] text-slate-500">
              <Clock3 size={11} /> {formatEpoch(kalib?.agitasi_epoch)}
            </div>
          </div>
        </div>

        <div className="rounded-xl border border-line bg-ink/30 p-3">
          <div className="mb-2 flex items-center justify-between gap-2 text-xs font-medium text-slate-300">
            <span>Sensor level</span>
            <span className="flex items-center gap-1 text-[10px] text-slate-500">
              <Clock3 size={11} /> {formatEpoch(kalib?.level_epoch)}
            </span>
          </div>
          <div className="space-y-1.5">
            {levelNames.map((name, index) => {
              const item = level[index];
              return (
                <div key={name} className="grid grid-cols-[1fr_auto_auto] items-center gap-2 rounded-lg border border-line/60 px-3 py-1.5 text-xs">
                  <span className="text-slate-200">{name}</span>
                  <span className="text-slate-500">
                    {item?.kosong ?? '--'} / {item?.penuh ?? '--'}
                  </span>
                  <Badge tone={item?.ok ? 'safe' : 'muted'}>{item?.ok ? 'OK' : '--'}</Badge>
                </div>
              );
            })}
          </div>
        </div>

        <div className="rounded-xl border border-line bg-ink/30 p-3">
          <div className="mb-2 flex items-center justify-between gap-2 text-xs font-medium text-slate-300">
            <span>Sensor suhu</span>
            <span className="flex items-center gap-1 text-[10px] text-slate-500">
              <Clock3 size={11} /> {formatEpoch(kalib?.suhu_epoch)}
            </span>
          </div>
          <div className="space-y-1.5">
            {tempNames.map((name, index) => {
              const item = suhu[index];
              return (
                <div key={name} className="flex items-center justify-between gap-2 rounded-lg border border-line/60 px-3 py-1.5 text-xs">
                  <span className="text-slate-200">{name}</span>
                  <span className="text-slate-400">
                    addr {item?.addr ?? '--'} · offset {(item?.offset_x10 ?? 0) / 10} C
                  </span>
                </div>
              );
            })}
          </div>
          <div className="mt-2 flex items-center justify-between gap-2 rounded-lg border border-line/60 px-3 py-1.5 text-xs">
            <span className="text-slate-200">MQ-135</span>
            <span className="text-slate-400">
              {kalib?.mq?.aktif ? 'ON' : 'OFF'} · {kalib?.mq?.normal ?? '--'} / {kalib?.mq?.kotor ?? '--'} ppm
            </span>
          </div>
          <div className="mt-1 flex items-center gap-1 text-[10px] text-slate-500">
            <Clock3 size={11} /> {formatEpoch(kalib?.mq_epoch)}
          </div>
        </div>
      </div>
    </Card>
  );
}
