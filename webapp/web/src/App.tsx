import { useEffect, useRef } from 'react';
import { LoaderCircle } from 'lucide-react';
import { connectWs } from './api';
import { isDemoMode } from './demo';
import { useApp } from './store';
import Login from './pages/Login';
import Shell from './pages/Shell';
import { Toast } from './components/ui';

export default function App() {
  const boot = useApp((state) => state.boot);
  const booting = useApp((state) => state.booting);
  const user = useApp((state) => state.user);
  const toast = useApp((state) => state.toast);
  const clearToast = useApp((state) => state.clearToast);
  const setToast = useApp((state) => state.setToast);
  const setSnapshot = useApp((state) => state.setSnapshot);
  const addEvent = useApp((state) => state.addEvent);
  const setConnection = useApp((state) => state.setConnection);

  const booted = useRef(false);
  useEffect(() => {
    // React StrictMode menjalankan effect dua kali di development.
    if (booted.current) return;
    booted.current = true;
    void boot();
  }, [boot]);

  useEffect(() => {
    if (!user || isDemoMode()) return;
    const close = connectWs(
      (message) => {
        switch (message.type) {
          case 'hello':
          case 'snapshot':
          case 'telemetry':
            setSnapshot(message.data);
            break;
          case 'event':
            addEvent(message.data);
            break;
          case 'ack':
            setToast(
              message.data.ok
                ? `${message.data.cmd.toUpperCase()} diterima: ${message.data.msg || 'OK'}`
                : `${message.data.cmd.toUpperCase()} gagal: ${message.data.msg || 'ditolak'}`,
            );
            break;
          case 'connection':
            if (!message.data.brokerConnected) setToast('Koneksi broker MQTT terputus.');
            break;
        }
      },
      setConnection,
    );
    return close;
  }, [user, addEvent, setConnection, setSnapshot, setToast]);

  useEffect(() => {
    if (!toast) return;
    const timer = window.setTimeout(clearToast, 4500);
    return () => window.clearTimeout(timer);
  }, [toast, clearToast]);

  if (booting) {
    return (
      <main className="flex min-h-dvh items-center justify-center text-slate-400">
        <LoaderCircle className="mr-2 animate-spin" size={18} /> Memuat dashboard...
      </main>
    );
  }

  return (
    <>
      {user ? <Shell user={user} /> : <Login />}
      <Toast message={toast} onClose={clearToast} />
    </>
  );
}
