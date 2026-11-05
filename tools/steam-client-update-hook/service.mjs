import fs from 'node:fs/promises';
import path from 'node:path';
import { connect, targets } from './cdp.mjs';
import { installHook } from './frontend.mjs';
import { checkClient, SUPPORTED_BUILD } from './backend.mjs';
import { verifyPinnedBuild } from './compatibility.mjs';
const bridgeName = '__ugmClientUpdateRequest';
const stateDir = path.join(process.env.HOME, '.local/state/ubuntu-gaming-mode/steam-client-update-hook');
await fs.mkdir(stateDir, { recursive: true, mode: 0o700 });
const statusPath = path.join(stateDir, 'status.json');
let active = null, stopping = false, lastMessage = '', busy = false;
async function status(value) {
  const data = { time: new Date().toISOString(), ...value };
  await fs.writeFile(statusPath + '.tmp', JSON.stringify(data, null, 2) + '\n', { mode: 0o600 });
  await fs.rename(statusPath + '.tmp', statusPath);
  const message = JSON.stringify(value);
  if (message !== lastMessage) { console.log(JSON.stringify(data)); lastMessage = message; }
}
async function inGamingMode() {
  for (const pid of await fs.readdir('/proc')) {
    if (!/^\d+$/.test(pid)) continue;
    try {
      const comm = (await fs.readFile(`/proc/${pid}/comm`, 'utf8')).trim();
      if (comm !== 'steam') continue;
      const args = (await fs.readFile(`/proc/${pid}/cmdline`, 'utf8')).split('\0');
      if (args.includes('-gamepadui')) return true;
    } catch { /* process exit */ }
  }
  return false;
}
async function detach() {
  if (!active) return;
  const client = active.client;
  active = null;
  try { await client.evaluate('window.__UGMClientUpdateHook?.remove()'); }
  finally { client.close(); }
}
async function attach(target) {
  const verifiedBuild = await verifyPinnedBuild();
  const client = await connect(target);
  let requestActive = false;
  client.onEvent(async event => {
    if (event.method !== 'Runtime.bindingCalled' || event.params.name !== bridgeName) return;
    let request, reply;
    try {
      request = JSON.parse(event.params.payload);
      if (!Number.isSafeInteger(request.id) || request.id <= 0 || request.op !== 'check' || request.installed !== SUPPORTED_BUILD) throw Error('Petición UGM inválida');
      if (requestActive) throw Error('Ya hay una consulta del cliente en curso');
      requestActive = true;
      try {
        const result = await checkClient(request.installed);
        console.log(JSON.stringify({ event: 'client-check', time: new Date().toISOString(), ...result }));
        reply = { ok: true, result };
      } finally { requestActive = false; }
    } catch (error) {
      console.error(JSON.stringify({ event: 'client-check-error', error: error.message }));
      reply = { ok: false, error: error.message };
    }
    if (!request || !Number.isSafeInteger(request.id)) return;
    try { await client.evaluate(`window.__UGMClientUpdateHook?.resolve(${request.id},${JSON.stringify(reply)})`); }
    catch (error) { console.error('Reply failed:', error.message); }
  });
  try {
    await client.call('Runtime.enable');
    await client.call('Runtime.addBinding', { name: bridgeName });
    const result = await client.evaluate(`(${installHook.toString()})(${JSON.stringify({ bridgeName, build: SUPPORTED_BUILD, verifiedBuild })})`);
    active = { client, targetId: target.id };
    await status({ state: 'attached', targetId: target.id, hook: result });
  } catch (error) { client.close(); throw error; }
}
async function tick() {
  if (busy || stopping) return;
  busy = true;
  try {
    if (!await inGamingMode()) {
      await detach(); await status({ state: 'waiting-gaming-mode' }); return;
    }
    const [target] = await targets();
    if (!target) { await detach(); await status({ state: 'waiting-shared-context' }); return; }
    if (active && active.targetId === target.id) {
      const hook = await active.client.evaluate('window.__UGMClientUpdateHook?.status()');
      if (hook) { await status({ state: 'attached', targetId: target.id, hook }); return; }
    }
    await detach(); await attach(target);
  } catch (error) {
    if (active) { active.client.close(); active = null; }
    await status({ state: 'waiting-or-incompatible', error: error.message });
  } finally { busy = false; }
}
let timer;
async function stop() {
  if (stopping) return;
  stopping = true; clearInterval(timer);
  try { await detach(); await status({ state: 'stopped' }); process.exit(0); }
  catch (error) { console.error('Cannot remove hook:', error.message); process.exit(1); }
}
process.on('SIGTERM', stop); process.on('SIGINT', stop);
await tick(); timer = setInterval(tick, 5000);
