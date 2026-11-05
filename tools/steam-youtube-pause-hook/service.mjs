import fs from 'node:fs/promises';
import path from 'node:path';
import {connect, targets} from './cdp.mjs';
import {hookExpression} from './runtime.mjs';
import {createSupervisor} from './supervisor.mjs';
const stateDir = path.join(process.env.XDG_STATE_HOME || path.join(process.env.HOME, '.local/state'), 'ubuntu-gaming-mode/steam-youtube-pause-hook');
await fs.mkdir(stateDir, {recursive: true, mode: 0o700});
const statusPath = path.join(stateDir, 'status.json');
let lastMessage = '';
async function report(value) {
  const data = {time: new Date().toISOString(), ...value};
  await fs.writeFile(statusPath + '.tmp', JSON.stringify(data, null, 2) + '\n', {mode: 0o600});
  await fs.rename(statusPath + '.tmp', statusPath);
  const message = JSON.stringify(value);
  if (message !== lastMessage) {console.log(JSON.stringify(data)); lastMessage = message;}
}
async function inGamingMode() {
  const uid = process.getuid();
  for (const entry of await fs.readdir('/proc')) {
    if (!/^\d+$/.test(entry)) continue;
    try {
      const dir = '/proc/' + entry;
      if ((await fs.stat(dir)).uid !== uid) continue;
      if ((await fs.readFile(dir + '/comm', 'utf8')).trim() !== 'steam') continue;
      if ((await fs.readFile(dir + '/cmdline', 'utf8')).split('\0').includes('-gamepadui')) return true;
    } catch { /* process exit or different user's inaccessible process */ }
  }
  return false;
}
const supervisor = createSupervisor({inGamingMode, targets, connect, report,
  apply: hookExpression('apply'), status: hookExpression('status'), remove: hookExpression('remove')});
let timer, stopping = false;
async function stop() {
  if (stopping) return;
  stopping = true; clearInterval(timer);
  try {await supervisor.stop(); process.exit(0);}
  catch (error) {console.error('Cannot remove YouTube hook:', error.message); process.exit(1);}
}
process.on('SIGTERM', stop); process.on('SIGINT', stop);
await supervisor.tick();
if (!stopping) timer = setInterval(() => {supervisor.tick().catch(error => {console.error(error); process.exit(1);});}, 2000);
