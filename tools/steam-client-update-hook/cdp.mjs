export async function connect(target) {
  const url = new URL(target.webSocketDebuggerUrl);
  if (url.protocol !== 'ws:' || !['127.0.0.1', 'localhost'].includes(url.hostname)) throw Error('CDP no local');
  const ws = new WebSocket(url);
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => { ws.close(); reject(Error('CDP connect timeout')); }, 5000);
    ws.onopen = () => { clearTimeout(timer); resolve(); };
    ws.onerror = () => { clearTimeout(timer); reject(Error('CDP connect error')); };
  });
  let sequence = 0;
  const pending = new Map();
  const listeners = new Set();
  ws.onmessage = event => {
    const message = JSON.parse(event.data);
    const request = pending.get(message.id);
    if (request) {
      clearTimeout(request.timer); pending.delete(message.id);
      message.error ? request.reject(Error(message.error.message)) : request.resolve(message.result);
    } else for (const listener of listeners) listener(message);
  };
  ws.onclose = () => {
    for (const request of pending.values()) { clearTimeout(request.timer); request.reject(Error('CDP disconnected')); }
    pending.clear();
  };
  function call(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = ++sequence;
      const timer = setTimeout(() => { pending.delete(id); reject(Error('CDP request timeout')); }, 20000);
      pending.set(id, { resolve, reject, timer });
      ws.send(JSON.stringify({ id, method, params }));
    });
  }
  return {
    call, onEvent: fn => listeners.add(fn), close: () => ws.close(),
    async evaluate(expression) {
      const result = await call('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
      if (result.exceptionDetails) throw Error(result.exceptionDetails.text + ': ' + (result.exceptionDetails.exception?.description || ''));
      return result.result.value;
    },
  };
}
export async function targets() {
  const response = await fetch('http://127.0.0.1:8080/json/list', { signal: AbortSignal.timeout(3000) });
  return (await response.json()).filter(t => t.type === 'page' && t.title === 'SharedJSContext' && new URL(t.url).origin === 'https://steamloopback.host');
}
if (process.argv[1]?.endsWith('/cdp.mjs')) {
  const [target] = await targets();
  if (!target) throw Error('SharedJSContext ausente');
  const client = await connect(target);
  try { console.log(JSON.stringify(await client.evaluate(process.argv[2]), null, 2)); }
  finally { client.close(); }
}
