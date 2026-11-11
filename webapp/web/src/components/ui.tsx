import type { ButtonHTMLAttributes, ReactNode } from 'react';

export function cn(...parts: Array<string | false | null | undefined>): string {
  return parts.filter(Boolean).join(' ');
}

export function Card({
  title,
  subtitle,
  action,
  children,
  className,
  danger,
}: {
  title?: string;
  subtitle?: string;
  action?: ReactNode;
  children: ReactNode;
  className?: string;
  danger?: boolean;
}) {
  return (
    <section
      className={cn(
        'rounded-2xl border bg-panel/90 p-4 shadow-lg shadow-black/10 backdrop-blur',
        danger ? 'border-danger/60' : 'border-line',
        className,
      )}
    >
      {(title || action) && (
        <div className="mb-3 flex items-start justify-between gap-3">
          <div>
            {title && <h2 className="text-sm font-semibold tracking-wide text-slate-100">{title}</h2>}
            {subtitle && <p className="mt-0.5 text-xs text-slate-400">{subtitle}</p>}
          </div>
          {action}
        </div>
      )}
      {children}
    </section>
  );
}

type Tone = 'safe' | 'warn' | 'danger' | 'muted' | 'info';

const toneClass: Record<Tone, string> = {
  safe: 'border-safe/30 bg-safe/10 text-safe',
  warn: 'border-warn/30 bg-warn/10 text-warn',
  danger: 'border-danger/40 bg-danger/10 text-danger',
  muted: 'border-line bg-white/5 text-slate-300',
  info: 'border-sky-400/30 bg-sky-400/10 text-sky-300',
};

export function Badge({ tone = 'muted', children }: { tone?: Tone; children: ReactNode }) {
  return (
    <span
      className={cn(
        'inline-flex items-center gap-1 rounded-full border px-2 py-0.5 text-[11px] font-medium',
        toneClass[tone],
      )}
    >
      {children}
    </span>
  );
}

type ButtonTone = 'primary' | 'secondary' | 'ghost' | 'danger' | 'warn';

const buttonTone: Record<ButtonTone, string> = {
  primary: 'bg-safe text-ink hover:bg-safe/90 disabled:bg-safe/30',
  secondary: 'border border-line bg-white/5 text-slate-100 hover:bg-white/10',
  ghost: 'text-slate-300 hover:bg-white/5',
  danger: 'bg-danger text-white hover:bg-danger/90 disabled:bg-danger/30',
  warn: 'bg-warn text-ink hover:bg-warn/90 disabled:bg-warn/30',
};

export function Button({
  tone = 'secondary',
  className,
  children,
  ...props
}: ButtonHTMLAttributes<HTMLButtonElement> & { tone?: ButtonTone }) {
  return (
    <button
      className={cn(
        'inline-flex min-h-10 items-center justify-center gap-2 rounded-xl px-4 py-2 text-sm font-semibold transition disabled:cursor-not-allowed disabled:opacity-50',
        buttonTone[tone],
        className,
      )}
      {...props}
    >
      {children}
    </button>
  );
}

export function Gauge({
  label,
  value,
  max,
  unit = 'ml',
  tone = 'safe',
}: {
  label: string;
  value: number;
  max: number;
  unit?: string;
  tone?: 'safe' | 'warn' | 'danger';
}) {
  const percent = max > 0 ? Math.max(0, Math.min(100, Math.round((value / max) * 100))) : 0;
  const fill = tone === 'danger' ? 'bg-danger' : tone === 'warn' ? 'bg-warn' : 'bg-safe';
  return (
    <div className="rounded-xl border border-line bg-ink/40 p-3">
      <div className="flex items-baseline justify-between gap-2">
        <span className="text-xs font-medium text-slate-300">{label}</span>
        <span className="text-sm font-semibold text-slate-100">
          {value}
          <span className="ml-1 text-[10px] font-normal text-slate-400">{unit}</span>
        </span>
      </div>
      <div className="mt-2 h-2 overflow-hidden rounded-full bg-white/10">
        <div className={cn('h-full rounded-full transition-all', fill)} style={{ width: `${percent}%` }} />
      </div>
      <div className="mt-1 flex justify-between text-[10px] text-slate-500">
        <span>{percent}%</span>
        <span>max {max}</span>
      </div>
    </div>
  );
}

export function Dialog({
  open,
  title,
  children,
  confirmLabel = 'Konfirmasi',
  confirmTone = 'danger',
  busy,
  onConfirm,
  onClose,
}: {
  open: boolean;
  title: string;
  children: ReactNode;
  confirmLabel?: string;
  confirmTone?: ButtonTone;
  busy?: boolean;
  onConfirm: () => void;
  onClose: () => void;
}) {
  if (!open) return null;
  return (
    <div className="fixed inset-0 z-50 flex items-end justify-center bg-black/60 p-3 sm:items-center">
      <div
        role="dialog"
        aria-modal="true"
        aria-label={title}
        className="w-full max-w-md rounded-2xl border border-line bg-panel p-5 shadow-2xl"
      >
        <h2 className="text-base font-semibold text-slate-100">{title}</h2>
        <div className="mt-2 text-sm leading-relaxed text-slate-300">{children}</div>
        <div className="mt-5 flex justify-end gap-2">
          <Button tone="ghost" onClick={onClose} disabled={busy}>
            Batal
          </Button>
          <Button tone={confirmTone} onClick={onConfirm} disabled={busy}>
            {busy ? 'Memproses...' : confirmLabel}
          </Button>
        </div>
      </div>
    </div>
  );
}

export function Toast({ message, onClose }: { message: string; onClose: () => void }) {
  if (!message) return null;
  return (
    <button
      onClick={onClose}
      className="fixed bottom-24 left-1/2 z-50 w-[calc(100%-2rem)] max-w-md -translate-x-1/2 rounded-xl border border-line bg-slate-900/95 px-4 py-3 text-left text-sm text-slate-100 shadow-2xl sm:bottom-6"
    >
      {message}
      <span className="ml-2 text-xs text-slate-500">tutup</span>
    </button>
  );
}
