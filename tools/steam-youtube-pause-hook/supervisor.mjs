// Lifecycle separated from I/O so restart/reload and shutdown can be verified.
export function createSupervisor({inGamingMode, targets, connect, report, apply, status, remove}) {
  let active = null, running = null, stopping = false;
  async function detach() {
    if (!active) return;
    const previous = active; active = null;
    try { await previous.client.evaluate(remove); }
    finally { previous.client.close(); }
  }
  async function step() {
    try {
      if (!await inGamingMode()) {
        await detach(); await report({state: 'waiting-gaming-mode'}); return;
      }
      const all = await targets();
      if (all.length !== 1) {
        await detach();
        await report({state: all.length ? 'ambiguous-shared-context' : 'waiting-shared-context'}); return;
      }
      const target = all[0];
      if (active && active.targetId === target.id) {
        const hook = await active.client.evaluate(status);
        if (hook?.active) { await report({state: 'attached', targetId: target.id, hook}); return; }
        // Same target ID may survive a context reload. Nothing remains to restore.
        active.client.close(); active = null;
      } else if (active) await detach();
      const client = await connect(target);
      try {
        const hook = await client.evaluate(apply);
        if (!hook?.active) throw Error('Hook no activo tras aplicar');
        active = {client, targetId: target.id};
        await report({state: 'attached', targetId: target.id, hook});
      } catch (error) {client.close(); throw error;}
    } catch (error) {
      // A dropped connection does not justify touching other Steam hooks.
      if (active) {active.client.close(); active = null;}
      await report({state: 'waiting-or-incompatible', error: error.message});
    }
  }
  return {
    tick() {
      if (stopping) return Promise.resolve();
      if (!running) running = step().finally(() => {running = null;});
      return running;
    },
    async stop() {
      stopping = true;
      if (running) await running;
      if (active) await detach();
      else if (await inGamingMode()) {
        // Retry removal when the preceding connection was lost but Steam survived.
        const all = await targets();
        if (all.length > 1) throw Error('No se puede revertir: SharedJSContext ambiguo');
        if (all.length === 1) {
          const client = await connect(all[0]);
          try {await client.evaluate(remove);} finally {client.close();}
        }
      }
      await report({state: 'stopped'});
    },
  };
}
