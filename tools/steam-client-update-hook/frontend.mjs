// Runs only inside Steam's SharedJSContext. No DOM patches or native API replacement.
export async function installHook({ bridgeName, build, verifiedBuild = null, version = '2' }) {
  if (window.__UGMClientUpdateHook) {
    if (window.__UGMClientUpdateHook.version !== version) throw Error('Ya hay otro hook UGM cargado');
    return window.__UGMClientUpdateHook.status();
  }
  const u = window.UpdateStore?.m_updater;
  if (!u || u.m_bUpdateStateForced || u.m_spoofer) throw Error('Actualizador no disponible o simulado');
  const reportedBuild = (await SteamClient.System.GetSystemInfo()).nSteamVersion;
  // Zero is a missing version, never a wildcard for another build. The service
  // supplies verifiedBuild only after hashing the pinned UI AND native client.
  const pinnedFallback = reportedBuild === 0 && verifiedBuild === build;
  if (reportedBuild !== build && !pinnedFallback) throw Error('Build de Steam no compatible');
  const buildSource = pinnedFallback ? 'pinned-client-files' : 'runtime';
  const names = ['CheckForUpdates', 'CheckForUpdatesBatched', 'SetUpdateState', 'GetAvailableTypes', 'GetRecheckAndInstallUpdateTypes', 'ApplyUpdate'];
  const descriptors = Object.fromEntries(names.map(k => [k, Object.getOwnPropertyDescriptor(u, k)]));
  const original = Object.fromEntries(names.map(k => [k, u[k]]));
  if (names.some(k => typeof original[k] !== 'function' || descriptors[k]?.configurable === false)) throw Error('Contrato del actualizador cambiado');
  if (!original.CheckForUpdates.toString().includes('SteamClient.Updates?.CheckForUpdates()') ||
      !original.ApplyUpdate.toString().includes('pe.add_apply_types(Ve)')) throw Error('Métodos del actualizador no compatibles');
  let nativeState = u.m_updateState;
  let mode = false, applying = false, inflight = null, removed = false;
  let last = null, lastError = null, checks = 0, applications = 0, sequence = 0;
  const pending = new Map();
  function request() {
    return new Promise((resolve, reject) => {
      const id = ++sequence;
      const timer = setTimeout(() => { pending.delete(id); reject(Error('El servicio UGM no ha respondido a la consulta del cliente')); }, 15000);
      pending.set(id, { resolve, reject, timer });
      try { window[bridgeName](JSON.stringify({ id, op: 'check', installed: build })); }
      catch (error) { clearTimeout(timer); pending.delete(id); reject(error); }
    });
  }
  function publish(state) { if (!removed) original.SetUpdateState.call(u, state); }
  function idleState(result, error) {
    return { state: result?.available ? 4 : 2, supports_os_updates: false,
      update_check_results: [{ type: 1, eresult: error ? 2 : 1, available: !!result?.available,
        rtime_checked: error ? undefined : (result?.checked || Math.floor(Date.now()/1000)),
        version_current: String(build), version_available: result ? String(result.latest) : '' }],
      update_apply_results: [] };
  }
  async function check(callback) {
    if (applying) return { result: 10, message: 'El cliente ya está aplicando una actualización' };
    if (!inflight) inflight = (async () => {
      mode = true; checks++; lastError = null;
      publish({ state: 3, supports_os_updates: false, update_check_results: [], update_apply_results: [] });
      try {
        last = await request();
        if (removed) return { result: 2, message: 'Hook desactivado' };
        publish(idleState(last));
        // Preserve a real client-only restart already reported by Steam.
        if (nativeState.state === 6 && nativeState.update_apply_results?.some(r => r.type === 1 && r.eresult === 1)) {
          publish({ ...idleState(last), state: 6, update_apply_results: nativeState.update_apply_results.filter(r => r.type === 1) });
        }
        return { result: 1, message: '' };
      } catch (error) {
        last = null; lastError = error.message;
        publish(idleState(null, error));
        return { result: 2, message: error.message };
      }
    })().finally(() => { inflight = null; });
    const result = await inflight;
    if (result.result !== 1 && callback) await callback({ eResult: result.result, strMsg: result.message });
    return result;
  }
  const replacements = {
    CheckForUpdates: check,
    CheckForUpdatesBatched: check,
    SetUpdateState(state) {
      nativeState = state;
      if (!mode) return original.SetUpdateState.call(u, state);
      if (!applying) return; // Background OS/BIOS results must not overwrite the manual client query.
      const clientChecks = (state.update_check_results || []).filter(r => r.type === 1);
      const clientApply = (state.update_apply_results || []).filter(r => r.type === 1);
      const projected = { ...state, supports_os_updates: false, update_check_results: clientChecks, update_apply_results: clientApply };
      // A client-only action must never request a system reboot.
      if (projected.state === 7 || projected.state === 8) {
        projected.state = clientApply.some(r => r.eresult === 1) ? 6 : 2;
      }
      publish(projected);
      if (projected.state === 6 || ([2, 4].includes(projected.state) && clientApply.some(r => r.eresult !== 1))) applying = false;
    },
    GetAvailableTypes() { return mode ? (u.m_updateState.update_check_results || []).filter(r => r.type === 1 && r.available).map(() => 1) : original.GetAvailableTypes.call(u); },
    GetRecheckAndInstallUpdateTypes() { return [1]; },
    async ApplyUpdate(_types, callback) {
      if (applying) return { result: 10, message: 'El cliente ya está aplicando una actualización' };
      const checked = await check(callback);
      if (checked.result !== 1) return checked;
      if (!last?.available) return { result: 1, message: 'El cliente Steam ya está actualizado' };
      applying = true; applications++;
      publish({ ...idleState(last), state: 5 });
      try {
        // Steam's own protobuf serializer and downloader. Only CLIENT=1 is passed.
        const result = await original.ApplyUpdate.call(u, [1], callback);
        if (result?.result !== 1) { applying = false; lastError = result?.message || 'Steam no pudo aplicar la actualización del cliente'; publish(idleState(last)); }
        return result;
      } catch (error) {
        applying = false; lastError = error.message; publish(idleState(last));
        if (callback) await callback({ eResult: 2, strMsg: error.message });
        return { result: 2, message: error.message };
      }
    },
  };
  const hook = {
    version,
    resolve(id, reply) {
      const item = pending.get(id);
      if (!item) return;
      clearTimeout(item.timer); pending.delete(id);
      reply.ok ? item.resolve(reply.result) : item.reject(Error(reply.error || 'Error de consulta del cliente Steam'));
    },
    status: () => ({ version, reportedBuild, installedBuild: build, buildSource, mode, applying, checks, applications, last, lastError }),
    remove() {
      if (applying) throw Error('No desactivar mientras Steam aplica una actualización');
      removed = true;
      for (const item of pending.values()) { clearTimeout(item.timer); item.reject(Error('Hook desactivado')); }
      pending.clear();
      for (const name of names) {
        if (u[name] !== replacements[name]) continue;
        descriptors[name] ? Object.defineProperty(u, name, descriptors[name]) : delete u[name];
      }
      // Restore the latest native state without triggering restart side effects.
      // SetUpdateState is a MobX action; a neutral state safely restores its fields.
      if (mode) original.SetUpdateState.call(u, { ...nativeState, state: 0 });
      if (mode) u.m_updateState.state = nativeState.state;
      delete window.__UGMClientUpdateHook;
      return { removed: true };
    },
  };
  try {
    for (const name of names) Object.defineProperty(u, name, { value: replacements[name], configurable: true, writable: true });
    window.__UGMClientUpdateHook = hook;
  } catch (error) { hook.remove(); throw error; }
  return hook.status();
}
