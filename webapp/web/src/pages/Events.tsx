import { useApp } from '../store';
import { Badge, Card } from '../components/ui';

function toneFor(kode: number): 'danger' | 'warn' | 'info' | 'muted' {
  if (kode >= 1 && kode <= 10) return 'danger';
  if (kode >= 130 && kode <= 132) return 'warn';
  if (kode >= 100) return 'info';
  return 'muted';
}

export default function Events() {
  const events = useApp((state) => state.events);

  return (
    <Card title="Event" subtitle={`${events.length} event terakhir di memori server`}>
      {events.length === 0 ? (
        <p className="text-sm text-slate-500">Belum ada event yang diterima.</p>
      ) : (
        <div className="divide-y divide-line">
          {events.map((event, index) => (
            <div key={`${event.deviceId}-${event.t}-${event.kode}-${index}`} className="flex gap-3 py-3">
              <Badge tone={toneFor(event.kode)}>{event.kode.toString().padStart(3, '0')}</Badge>
              <div className="min-w-0 flex-1">
                <div className="break-words text-sm text-slate-200">{event.detail || '-'}</div>
                <div className="mt-1 flex flex-wrap gap-x-3 gap-y-1 text-[11px] text-slate-500">
                  <span>{event.epoch > 0 ? new Date(event.epoch * 1000).toLocaleString() : `${event.t} ms`}</span>
                  <span>{event.deviceId}</span>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </Card>
  );
}
