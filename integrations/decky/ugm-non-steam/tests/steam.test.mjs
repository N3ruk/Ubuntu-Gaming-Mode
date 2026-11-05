import { test } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { pathToFileURL } from 'node:url';
import { resolve } from 'node:path';

execFileSync(process.execPath, ['node_modules/typescript/bin/tsc', 'src/steam.ts', 'src/model.ts', 'src/types.ts',
  '--target', 'ES2020', '--module', 'ESNext', '--moduleResolution', 'Bundler', '--strict', '--skipLibCheck', '--outDir', '.test-build']);
const { SteamBridge, diskStatus, quote } = await import(pathToFileURL(resolve('.test-build/steam.js')).href);
const { createModel } = await import(pathToFileURL(resolve('.test-build/model.js')).href);
const request = () => ({ name: 'Game Ω', exe: '/games/a b/game', startDir: '/games/a b', launchOptions: '--flag "two words"', compatTool: '', windows: false });
function fixture() {
  const rows = new Map();
  const calls = [];
  let account = '76561197960265851';
  let nextId = 0xf123abcd;
  let ignoredField = '';
  let skipUnregister = 0;
  const set = (id, field, value) => { calls.push([field, id, value]); if (field !== ignoredField) rows.get(id)[field] = value; };
  const apps = {
    async AddShortcut(name, exe, start, args) {
      calls.push(['add', name, exe, start, args]);
      rows.set(nextId, { strDisplayName: name, strShortcutExe: exe, strShortcutStartDir: start, strShortcutLaunchOptions: args });
      return nextId;
    },
    SetShortcutName: (id, value) => set(id, 'strDisplayName', value),
    SetShortcutExe: (id, value) => set(id, 'strShortcutExe', value),
    SetShortcutStartDir: (id, value) => set(id, 'strShortcutStartDir', value),
    SetShortcutLaunchOptions: (id, value) => set(id, 'strShortcutLaunchOptions', value),
    SetAppLaunchOptions: (id, value) => set(id, 'strLaunchOptions', value),
    SpecifyCompatTool: (id, value) => set(id, 'strCompatToolName', value),
    RemoveShortcut: id => { calls.push(['remove', id]); rows.delete(id); },
    RegisterForAppDetails: (id, callback) => {
      callback(rows.get(id)); // synchronous callback exercises unsubscribe cleanup
      return { unregister() { skipUnregister++; } };
    },
  };
  const runtime = { apps, account: () => account,
    overviews: () => [...rows.keys()].map(id => ({ appid: id, app_type: 1073741824, display_name: rows.get(id).strDisplayName })),
    overview: id => rows.has(id) ? { appid: id, display_name: rows.get(id).strDisplayName, app_type: 1073741824 } : null,
    sleep: async () => {},
  };
  return { rows, calls, apps, runtime, bridge: new SteamBridge(runtime), setAccount: value => { account = value; },
    setId: value => { nextId = value; }, ignore: field => { ignoredField = field; }, unregisters: () => skipUnregister };
}

test('native shortcut writes after registration and verifies all core fields', async () => {
  const f = fixture(), d = request();
  const result = await f.bridge.add(d);
  assert.equal(result.verified, true);
  assert.equal(f.calls[0][0], 'add');
  assert.equal(f.rows.get(result.appId).strShortcutExe, '"/games/a b/game"');
  assert.equal(f.rows.get(result.appId).strShortcutStartDir, d.startDir);
  assert.ok(f.unregisters() > 0);
});
test('Proton is explicitly set and read back', async () => {
  const f = fixture();
  const result = await f.bridge.add({ ...request(), windows: true, compatTool: 'proton_experimental' });
  assert.equal(result.verified, true);
  assert.equal(f.rows.get(result.appId).strCompatToolName, 'proton_experimental');
});
test('silently ignored launch options cannot claim verification', async () => {
  const f = fixture(); f.ignore('strShortcutLaunchOptions');
  const result = await f.bridge.add(request());
  assert.equal(result.verified, false);
  assert.ok(result.warnings.length);
  assert.equal(f.calls.filter(c => c[0] === 'add').length, 1);
});
test('silently ignored Proton cannot claim verification', async () => {
  const f = fixture(); f.ignore('strCompatToolName');
  assert.equal((await f.bridge.add({ ...request(), compatTool: 'proton' })).verified, false);
});
test('duplicate live shortcut is rejected without mutations', async () => {
  const f = fixture(); await f.bridge.add(request()); f.calls.length = 0;
  await assert.rejects(f.bridge.add(request()), /ya está en tu biblioteca/);
  assert.equal(f.calls.length, 0);
});
test('different launch options allow multiple apps using the same binary', async () => {
  const f = fixture(); await f.bridge.add(request()); f.setId(0xf123abce);
  assert.equal((await f.bridge.add({ ...request(), launchOptions: '--other-app' })).verified, true);
});
test('missing setter is detected before AddShortcut', async () => {
  const f = fixture(); delete f.apps.SetShortcutStartDir;
  await assert.rejects(f.bridge.add(request()), /SetShortcutStartDir/);
  assert.equal(f.calls.length, 0);
});
test('missing Proton API is detected before AddShortcut', async () => {
  const f = fixture(); delete f.apps.SpecifyCompatTool;
  await assert.rejects(f.bridge.add({ ...request(), compatTool: 'proton' }), /SpecifyCompatTool/);
  assert.equal(f.calls.length, 0);
});
test('quoted fallback only occurs after an explicit zero response', async () => {
  const f = fixture(), add = f.apps.AddShortcut;
  let calls = 0;
  f.apps.AddShortcut = async (...args) => ++calls === 1 ? 0 : add(...args);
  assert.equal((await f.bridge.add(request())).verified, true);
  assert.equal(calls, 2);
});
test('an AddShortcut exception never triggers a blind retry', async () => {
  const f = fixture(); let calls = 0;
  f.apps.AddShortcut = async () => { calls++; throw new Error('transport lost'); };
  await assert.rejects(f.bridge.add(request()), /podría haber creado/);
  assert.equal(calls, 1);
});
test('unexpected and oversized app identifiers are rejected', async () => {
  for (const id of [undefined, '123', Number.NaN, 2 ** 32 + 12]) {
    const f = fixture(); f.apps.AddShortcut = async () => id;
    await assert.rejects(f.bridge.add(request()), /Respuesta inesperada/);
    assert.equal(f.calls.length, 0);
  }
});
test('existing ID returned by Steam is never modified or owned', async () => {
  const f = fixture(); await f.bridge.add(request()); f.calls.length = 0;
  f.apps.AddShortcut = async () => 0xf123abcd;
  await assert.rejects(f.bridge.add({ ...request(), exe: '/other/game' }), /ya existía/);
  assert.equal(f.calls.length, 0);
});
test('partial failures keep created ID visible without retrying creation', async () => {
  const f = fixture(); f.apps.SetShortcutName = () => { throw new Error('setter failed'); };
  const result = await f.bridge.add(request());
  assert.equal(result.appId, 0xf123abcd);
  assert.equal(result.verified, false);
  assert.match(result.warnings[0], /setter failed/);
});
test('account changes during creation stop setters and prevent undo', async () => {
  const f = fixture(), add = f.apps.AddShortcut;
  f.apps.AddShortcut = async (...args) => { const id = await add(...args); f.setAccount('76561197960265852'); return id; };
  await assert.rejects(f.bridge.add(request()), /cuenta Steam ha cambiado/);
  assert.deepEqual(f.calls.map(c => c[0]), ['add']);
});
test('undo removes only this verified created shortcut', async () => {
  const f = fixture(); const result = await f.bridge.add(request());
  await assert.rejects(f.bridge.undo({ ...result }), /operación actual/);
  await f.bridge.undo(result);
  assert.equal(f.rows.size, 0);
  assert.deepEqual(f.calls.at(-1), ['remove', result.appId]);
});
test('undo refuses a shortcut changed outside the plugin', async () => {
  const f = fixture(); const result = await f.bridge.add(request());
  f.rows.get(result.appId).strShortcutLaunchOptions = '--changed';
  await assert.rejects(f.bridge.undo(result), /ha cambiado/);
  assert.ok(f.rows.has(result.appId));
});
test('undo refuses cross-account operations', async () => {
  const f = fixture(); const result = await f.bridge.add(request()); f.setAccount('76561197960265852');
  await assert.rejects(f.bridge.undo(result), /cuenta Steam ha cambiado/);
  assert.ok(f.rows.has(result.appId));
});
test('duplicate check fails closed if existing details are incomplete', async () => {
  const f = fixture(); await f.bridge.add(request());
  delete f.rows.get(0xf123abcd).strShortcutLaunchOptions;
  delete f.rows.get(0xf123abcd).strLaunchOptions;
  f.calls.length = 0;
  await assert.rejects(f.bridge.add({ ...request(), exe: '/another/game' }), /descartar un duplicado/);
  assert.equal(f.calls.length, 0);
});
test('in-flight additions are serialized even with double-clicks', async () => {
  const f = fixture(); let resume;
  const original = f.apps.AddShortcut;
  f.apps.AddShortcut = (...args) => new Promise(resolve => { resume = async () => resolve(await original(...args)); });
  const first = f.bridge.add(request());
  await assert.rejects(f.bridge.add(request()), /operación en curso/);
  await resume();
  assert.equal((await first).verified, true);
});
test('disk status compares ID, name, executable, working directory and arguments', async () => {
  const f = fixture(); const result = await f.bridge.add(request());
  const saved = { ...result.request, appId: result.appId };
  assert.equal(diskStatus(result, [saved]), 'saved');
  for (const field of ['appId', 'name', 'exe', 'startDir', 'launchOptions']) {
    assert.equal(diskStatus(result, [{ ...saved, [field]: field === 'appId' ? 42 : 'different' }]), 'pending');
  }
  assert.equal(diskStatus(result, []), 'pending');
});
test('command quoting keeps literal dollars, backticks and spaces', () => {
  assert.equal(quote('/games/$d/`game` x'), '"/games/\\$d/\\`game\\` x"');
});
test('QAM remount preserves draft while account reset clears all private state', () => {
  const m = createModel(); m.reset('account-one');
  let changes = 0; const off = m.subscribe(() => changes++);
  m.patch({ draft: request(), screen: 'form', busy: true }); off();
  assert.equal(m.get().draft.name, 'Game Ω');
  const off2 = m.subscribe(() => changes++);
  assert.equal(m.get().screen, 'form'); // new component subscription, same state
  m.reset('account-two'); off2();
  assert.equal(m.get().draft, null); assert.equal(m.get().busy, false);
  assert.equal(changes, 2);
});
