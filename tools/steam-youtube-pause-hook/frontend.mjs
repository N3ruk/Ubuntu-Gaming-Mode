// Session-only patch. The launcher pins the complete shipped module by SHA-256.
export function installPauseHook(proto, expected) {
  const names = ['shouldComponentUpdate', 'componentDidUpdate', 'CreatePlayer', 'DestroyPlayer', 'OnPlayerReady', 'componentWillUnmount'];
  const descriptors = Object.fromEntries(names.map(k => [k, Object.getOwnPropertyDescriptor(proto, k)]));
  for (const name of names) {
    const d = descriptors[name];
    if (!d?.configurable || (!d.get && typeof d.value !== 'function')) throw Error('Contrato incompatible: ' + name);
    if (!d.get && d.value.toString() !== expected[name]) throw Error('Método incompatible: ' + name);
  }
  let active = true;
  const states = new WeakMap(), bound = new WeakMap();
  const counts = {orders: 0, deferred: 0, commands: 0, staleReady: 0, avoidedRenders: 0};
  const events = [];
  function state(c) {
    if (!states.has(c)) states.set(c, {generation: 0, pending: false, desired: false, dead: false});
    return states.get(c);
  }
  function record(kind, c, extra = {}) {
    events.push({at: new Date().toISOString(), kind, video: c.props?.video, generation: state(c).generation, ...extra});
    if (events.length > 120) events.shift();
  }
  function apply(c) {
    const s = state(c), p = c.m_player, method = s.desired ? 'pauseVideo' : 'playVideo';
    if (!active || s.dead || !s.pending) return;
    if (!p || !c.m_bPlayerReady || typeof p[method] !== 'function') {
      counts.deferred++; record('deferred', c, {method, ready: c.m_bPlayerReady, available: typeof p?.[method]}); return;
    }
    // No asynchronous closure: always use the current instance and latest intent.
    p[method](); s.pending = false; counts.commands++;
    record('command', c, {method, reportedState: p.playerInfo?.playerState});
  }
  function order(c, desired) {
    const s = state(c); s.desired = !!desired; s.pending = true; counts.orders++; apply(c);
  }
  const handlers = {
    shouldComponentUpdate(original, args) {
      const next = args[0], changed = this.props.forcePause != next.forcePause;
      const result = original.apply(this, args);
      if (changed) {
        order(this, next.forcePause);
        // Pinned Steam contract: true is returned ONLY for a forcePause change.
        // Playback commands do not need a React render/ref rebind/recreation.
        if (result) {counts.avoidedRenders++; return false;}
      }
      return result;
    },
    componentDidUpdate(_original, args) {
      if (args[0].forcePause != this.props.forcePause) order(this, this.props.forcePause);
    },
    CreatePlayer(original, args) {
      const result = original.apply(this, args), s = state(this);
      s.dead = false; s.desired = !!args[0].forcePause; s.pending = s.desired;
      record('created', this); return result;
    },
    DestroyPlayer(original, args) {
      const s = state(this); s.generation++; s.pending = false;
      try {return original.apply(this, args);} finally {this.m_bPlayerReady = false; record('destroyed', this);}
    },
    OnPlayerReady(original, args) {
      const s = state(this);
      if (s.dead || !args[0]?.target || args[0].target !== this.m_player) {
        counts.staleReady++; record('stale-ready', this); return;
      }
      const result = original.apply(this, args);
      // Steam's original ready may autoplay. Reconcile pause AFTER that callback.
      if (this.props.forcePause && !s.pending) {s.pending = true; s.desired = true;}
      apply(this); record('ready', this); return result;
    },
    componentWillUnmount(original, args) {
      const s = state(this); s.dead = true; s.pending = false;
      return original.apply(this, args);
    },
  };
  const installed = {};
  for (const name of names) {
    const d = descriptors[name];
    if (d.get) {
      const get = function () {
        let cache = bound.get(this); if (!cache) bound.set(this, cache = new Map());
        if (!cache.has(name)) {
          const original = d.get.call(this), receiver = this;
          cache.set(name, function (...args) {return active ? handlers[name].call(receiver, original, args) : original(...args);});
        }
        return cache.get(name);
      };
      installed[name] = {...d, get};
    } else {
      const value = function (...args) {return active ? handlers[name].call(this, d.value, args) : d.value.apply(this, args);};
      installed[name] = {...d, value};
    }
    Object.defineProperty(proto, name, installed[name]);
  }
  return {
    status: () => ({active, version: 'experimental-1', counts: {...counts}, events: [...events]}),
    remove() {
      active = false;
      for (const name of names) {
        const d = Object.getOwnPropertyDescriptor(proto, name), ours = installed[name];
        if (d?.get === ours.get && d?.value === ours.value) Object.defineProperty(proto, name, descriptors[name]);
      }
      return {active: false};
    },
  };
}
