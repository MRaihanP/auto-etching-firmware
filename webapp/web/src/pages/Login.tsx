import { useState, type FormEvent } from 'react';
import { LockKeyhole, Radio, ShieldCheck } from 'lucide-react';
import { useApp } from '../store';
import { Button } from '../components/ui';

export default function Login() {
  const login = useApp((state) => state.login);
  const error = useApp((state) => state.error);
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    setBusy(true);
    try {
      await login(username, password);
    } catch {
      // Pesan sudah diteruskan store.
    } finally {
      setBusy(false);
    }
  };

  return (
    <main className="flex min-h-dvh items-center justify-center p-4">
      <div className="w-full max-w-md">
        <div className="mb-6 flex items-center gap-3">
          <div className="grid h-12 w-12 place-items-center rounded-2xl border border-safe/30 bg-safe/10 text-safe">
            <Radio size={24} />
          </div>
          <div>
            <h1 className="text-xl font-bold tracking-tight text-slate-100">Auto Etching V1</h1>
            <p className="text-xs text-slate-400">Dashboard lokal untuk mesin etching PCB</p>
          </div>
        </div>

        <form onSubmit={submit} className="rounded-2xl border border-line bg-panel p-5 shadow-2xl">
          <div className="mb-4 flex items-center gap-2 text-sm font-semibold text-slate-200">
            <LockKeyhole size={16} className="text-safe" />
            Masuk operator
          </div>

          <label className="mb-3 block text-xs font-medium text-slate-400">
            Username
            <input
              value={username}
              onChange={(event) => setUsername(event.target.value)}
              autoComplete="username"
              required
              className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2.5 text-sm text-slate-100 outline-none focus:border-safe"
            />
          </label>
          <label className="mb-4 block text-xs font-medium text-slate-400">
            Password
            <input
              type="password"
              value={password}
              onChange={(event) => setPassword(event.target.value)}
              autoComplete="current-password"
              required
              className="mt-1 w-full rounded-xl border border-line bg-ink px-3 py-2.5 text-sm text-slate-100 outline-none focus:border-safe"
            />
          </label>

          {error && (
            <p className="mb-3 rounded-lg border border-danger/30 bg-danger/10 px-3 py-2 text-xs text-danger">
              {error}
            </p>
          )}

          <Button tone="primary" className="w-full" disabled={busy}>
            {busy ? 'Menghubungkan...' : 'Masuk'}
          </Button>

          <p className="mt-4 flex items-start gap-2 text-[11px] leading-relaxed text-slate-500">
            <ShieldCheck size={14} className="mt-0.5 shrink-0 text-safe" />
            Browser hanya terhubung ke server dashboard. Kredensial MQTT tidak pernah dikirim ke
            browser.
          </p>
        </form>

        <p className="mt-4 text-center text-[11px] text-slate-500">
          Admin pertama dibuat lewat <code className="text-slate-400">pnpm seed</code> dari folder
          <code className="ml-1 text-slate-400">webapp</code>.
        </p>
      </div>
    </main>
  );
}
