import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import { manifestVersion, checkClient, SUPPORTED_BUILD as build } from './backend.mjs';
import { installHook } from './frontend.mjs';
const manifest = v => `"steamrt64" { "version" "${v}" "pkg" { "version" "other" } } "kvsign2" { "steamrt64" "abc" }`;
test('Valve manifest: root version, signatures and strict parsing', () => {
  assert.equal(manifestVersion(manifest(build)), build);
  for (const text of ['', '"steamrt64" { "version" "123" }', manifest(build).replace('"pkg"', '"version"'), manifest(build).slice(0,-2), manifest(build).replace('steamrt64','ubuntu')]) assert.throws(() => manifestVersion(text));
});
async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'ugm-update-hook-'));
  t.after(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'package'));
  await fs.writeFile(path.join(root, 'package/beta'), 'steamdeck_stable');
  await fs.writeFile(path.join(root, 'package/steam_client_steamdeck_stable_steamrt64.installed'), '');
  return root;
}
test('Backend chooses real branch and detects current/newer versions', async t => {
  const root = await fixture(t);
  for (const latest of [build, build+1]) {
    const result = await checkClient(build, { root, fetcher: async (url, options) => {
      assert.equal(url, 'https://client-update.fastly.steamstatic.com/steam_client_steamdeck_stable_steamrt64');
      assert.equal(options.redirect, 'error'); return new Response(manifest(latest));
    } });
    assert.equal(result.available, latest > build); assert.equal(result.latest, latest);
  }
});
test('Backend errors never become up-to-date: network, HTTP, invalid, downgrade, oversized', async t => {
  const root = await fixture(t);
  for (const fetcher of [async()=>{throw Error('offline');},async()=>new Response('',{status:503}),async()=>new Response('invalid'),async()=>new Response(manifest(build-1)),async()=>new Response('x'.repeat(262145))]) {
    await assert.rejects(checkClient(build, { root, fetcher }));
  }
});
test('Backend rejects branch race', async t => {
  const root = await fixture(t);
  await assert.rejects(checkClient(build, { root, fetcher: async () => {
    await fs.writeFile(path.join(root,'package/beta'), 'publicbeta');
    await fs.writeFile(path.join(root,'package/steam_client_publicbeta_steamrt64.installed'), '');
    return new Response(manifest(build));
  } }), /rama ha cambiado/);
});
function runtime({ available = false, failure = false, reportedBuild = build, verifiedBuild = null, clientResult = 1, nativeResult = 1, applyThrows = false, deferState = false } = {}) {
  const calls = { nativeCheck: 0, nativeApply: [], requests: 0, restarts: 0 };
  const originalState = {state:2,supports_os_updates:true,update_check_results:[{type:1,eresult:1,available:false},{type:2,eresult:2},{type:3,eresult:2}],update_apply_results:[]};
  class Updater {
    m_updateState = originalState;
    async CheckForUpdates(){ calls.nativeCheck++; /* SteamClient.Updates?.CheckForUpdates() */ }
    CheckForUpdatesBatched(){return this.CheckForUpdates();}
    SetUpdateState(s){this.m_updateState=s;if(s.state===7)calls.restarts++;}
    GetAvailableTypes(){return [1,2,3];}
    GetRecheckAndInstallUpdateTypes(){return [1,2];}
    GetMostSpecificApplyResult(){ return this.m_updateState.update_apply_results?.find(r=>r.eresult!==1)?.eresult ?? 1; }
    async ApplyUpdate(types, callback){ /* pe.add_apply_types(Ve) */
      calls.nativeApply.push(types);
      if (applyThrows) throw Error('native apply failed');
      calls.emitApply = () => this.SetUpdateState({state:clientResult===1?6:2,update_check_results:[{type:1,eresult:1},{type:2,eresult:2}],update_apply_results:[{type:1,eresult:clientResult},{type:2,eresult:2}]});
      if (!deferState) calls.emitApply();
      // Mirrors Steam's original aggregation and callback, rather than assuming
      // a successful native return cannot also raise the spurious OS dialog.
      const error = nativeResult===1 ? this.GetMostSpecificApplyResult() : nativeResult;
      if (error!==1 && callback) await callback({eResult:error,strMsg:'native result'});
      return {result:nativeResult,message:nativeResult===1?'':'native result'};
    }
  }
  const u = new Updater();
  const window = {UpdateStore:{m_updater:u}};
  window.bridge = text => {
    calls.requests++;
    const {id,installed}=JSON.parse(text); assert.equal(installed,build);
    queueMicrotask(()=>window.__UGMClientUpdateHook.resolve(id, failure ? {ok:false,error:'offline'} : {ok:true,result:{installed:build,latest:available?build+1:build,available,checked:100}}));
  };
  const context = vm.createContext({window,SteamClient:{System:{GetSystemInfo:async()=>({nSteamVersion:reportedBuild})}},setTimeout,clearTimeout,console});
  return { u, window, calls, originalState, install:()=>vm.runInContext(`(${installHook.toString()})({bridgeName:'bridge',build:${build},verifiedBuild:${JSON.stringify(verifiedBuild)}})`,context) };
}
test('Hook bypasses combined checker, coalesces and filters only updater state', async () => {
  const r=runtime();await r.install();
  await Promise.all([r.u.CheckForUpdates(),r.u.CheckForUpdates()]);
  assert.equal(r.calls.nativeCheck,0);assert.equal(r.calls.requests,1);
  assert.equal(r.u.m_updateState.state,2);
  assert.deepEqual(Array.from(r.u.m_updateState.update_check_results,x=>x.type),[1]);
  const state=r.u.m_updateState;
  r.u.SetUpdateState({state:7,update_apply_results:[{type:3,eresult:1}]});
  assert.equal(r.u.m_updateState,state);assert.equal(r.calls.restarts,0);
});
test('Hook reports error callback, does not invoke combined checker or installer', async () => {
  const r=runtime({failure:true});await r.install();let error;
  const result=await r.u.CheckForUpdates(e=>error=e);
  assert.equal(result.result,2);assert.equal(error.strMsg,'offline');
  assert.equal(r.u.m_updateState.update_check_results[0].eresult,2);
  await r.u.ApplyUpdate([1,2,3]);assert.equal(r.calls.nativeApply.length,0);assert.equal(r.calls.nativeCheck,0);
});
test('Apply rechecks then passes only CLIENT=1 to original method; no OS reboot', async () => {
  const r=runtime({available:true});await r.install();await r.u.ApplyUpdate([1,2,3]);
  assert.deepEqual(r.calls.nativeApply.map(x=>Array.from(x)),[[1]]);
  assert.equal(r.u.m_updateState.state,6);assert.equal(r.u.m_updateState.update_apply_results.length,1);
  assert.equal(r.calls.nativeCheck,0);assert.equal(r.calls.restarts,0);
});
test('Up-to-date apply does not download, exact method descriptors restored', async () => {
  const r=runtime();const before=Object.getOwnPropertyDescriptors(r.u);await r.install();await r.u.ApplyUpdate([2,3]);
  assert.equal(r.calls.nativeApply.length,0);
  r.window.__UGMClientUpdateHook.remove();
  assert.equal(Object.hasOwn(r.u,'CheckForUpdates'),false);
  assert.equal(Object.getOwnPropertyDescriptor(r.u,'CheckForUpdates'),before.CheckForUpdates);
  assert.deepEqual(JSON.parse(JSON.stringify(r.u.m_updateState)),r.originalState);
  assert.equal(r.window.__UGMClientUpdateHook,undefined);
});

test('Zero version requires pinned-file proof; never accepts another nonzero build', async () => {
  for (const [reportedBuild,verifiedBuild] of [[0,null],[0,build+1],[build+1,build],['0',build]]) {
    const r=runtime({reportedBuild,verifiedBuild});
    await assert.rejects(r.install(),/Build de Steam no compatible/);
    assert.equal(Object.hasOwn(r.u,'CheckForUpdates'),false);
  }
  await assert.rejects(runtime({reportedBuild:null,verifiedBuild:build}).install(),/Build de Steam no compatible/);
});
test('Verified zero version uses installed build and does not offer same-version reinstall', async () => {
  const r=runtime({reportedBuild:0,verifiedBuild:build});const status=await r.install();
  assert.equal(status.reportedBuild,0);assert.equal(status.installedBuild,build);
  assert.equal(status.buildSource,'pinned-client-files');
  await r.u.CheckForUpdates();await r.u.ApplyUpdate([1,2]);
  assert.equal(r.u.m_updateState.state,2);assert.equal(r.calls.nativeCheck,0);
  assert.equal(r.calls.nativeApply.length,0);
  assert.equal(r.u.m_updateState.update_check_results[0].version_current,String(build));
});
test('Available version with zero runtime build: client download and install without OS error callback', async () => {
  const r=runtime({reportedBuild:0,verifiedBuild:build,available:true});await r.install();
  let errors=[];await r.u.CheckForUpdates(e=>errors.push(e));
  assert.equal(r.u.m_updateState.state,4);
  assert.deepEqual(Array.from(r.u.GetAvailableTypes()),[1]);
  assert.deepEqual(Array.from(r.u.GetRecheckAndInstallUpdateTypes()),[1]);
  await r.u.ApplyUpdate([1,2,3],e=>errors.push(e));
  assert.equal(errors.length,0);assert.equal(r.u.m_updateState.state,6);
  assert.equal(r.calls.nativeCheck,0);assert.equal(r.calls.restarts,0);
  assert.deepEqual(r.calls.nativeApply.map(x=>Array.from(x)),[[1]]);
});
test('Real client apply failure stays visible in state and callback despite native success', async () => {
  const r=runtime({available:true,clientResult:2});await r.install();let error;
  await r.u.ApplyUpdate([1,2],e=>error=e);
  assert.equal(error.eResult,2);assert.equal(r.u.m_updateState.update_apply_results[0].eresult,2);
  assert.equal(r.window.__UGMClientUpdateHook.status().applying,false);
});
test('Native apply return failure and exception are not hidden', async () => {
  for (const options of [{nativeResult:2},{applyThrows:true}]) {
    const r=runtime({available:true,...options});await r.install();let errors=[];
    const result=await r.u.ApplyUpdate([1,2],e=>errors.push(e));
    assert.equal(result.result,2);assert.equal(errors.length,1);assert.equal(errors[0].eResult,2);
    assert.equal(r.window.__UGMClientUpdateHook.status().applying,false);
    assert.ok(r.window.__UGMClientUpdateHook.status().lastError);
  }
});
test('Delayed mixed native completion projects client only; cannot remove or apply twice in progress', async () => {
  const r=runtime({available:true,deferState:true});await r.install();
  await r.u.ApplyUpdate([1,2]);
  assert.equal(r.window.__UGMClientUpdateHook.status().applying,true);
  assert.throws(()=>r.window.__UGMClientUpdateHook.remove(),/No desactivar/);
  assert.equal((await r.u.ApplyUpdate([1])).result,10);
  r.u.SetUpdateState({state:5,update_apply_results:[{type:2,eresult:2}]});
  assert.equal(r.window.__UGMClientUpdateHook.status().applying,true);
  r.calls.emitApply();
  assert.equal(r.u.m_updateState.state,6);
  assert.deepEqual(Array.from(r.u.m_updateState.update_apply_results,x=>x.type),[1]);
  assert.equal(r.window.__UGMClientUpdateHook.status().applying,false);
  r.u.SetUpdateState({state:7,update_apply_results:[{type:2,eresult:1}]});
  assert.equal(r.u.m_updateState.state,6);assert.equal(r.calls.restarts,0);
});
test('Spoofed/forced updater and changed method contract remain rejected', async () => {
  for (const property of ['m_bUpdateStateForced','m_spoofer']) {
    const r=runtime({reportedBuild:0,verifiedBuild:build});r.u[property]=true;
    await assert.rejects(r.install(),/no disponible o simulado/);
  }
  const r=runtime({reportedBuild:0,verifiedBuild:build});
  r.u.ApplyUpdate=async()=>({result:1});
  await assert.rejects(r.install(),/no compatibles/);
});
