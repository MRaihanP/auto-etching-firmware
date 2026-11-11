import { config } from './config.js';
import { countUsers, upsertAdmin } from './db.js';

const username = config.adminUser.trim();
const password = config.adminPassword;

if (!username || !password) {
  console.error('ADMIN_USER dan ADMIN_PASSWORD wajib diisi di server/.env');
  process.exit(1);
}
if (password.length < 12) {
  console.error('ADMIN_PASSWORD minimal 12 karakter');
  process.exit(1);
}

const before = countUsers();
const user = upsertAdmin(username, password);
console.log(
  `${before === 0 ? 'Admin dibuat' : 'Admin diperbarui'}: ${user.username} (${user.role})`,
);
