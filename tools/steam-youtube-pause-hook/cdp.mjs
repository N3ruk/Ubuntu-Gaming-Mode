// Independent local transport; derived from the UGM client-update transport.
export async function connect(target) {
  const url = new URL(target.webSocketDebuggerUrl);
  if (url.protocol !== 'ws:' || !['127.0.0.1', 'localhost'].includes(url.hostname) || url.port !== '8080') throw Error('CDP no local o puerto inesperado');
  const ws = new WebSocket(url);
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => { ws.close(); reject(Error('CDP connect timeout')); }, 5000);
    ws.onopen = () => { clearTimeout(timer); resolve(); };
    ws.onerror = () => { clearTimeout(timer); reject(Error('CDP connect error')); };
    ws.onclose = () => { clearTimeout(timer); reject(Error('CDP closed before connect')); };
  });
  let sequence = 0;
  const pending = new Map();
  ws.onmessage = event => {
    let message;
    try { message = JSON.parse(event.data); } catch { return; }
    const request = pending.get(message.id);
    if (!request) return;
    clearTimeout(request.timer); pending.delete(message.id);
    message.error ? request.reject(Error(message.error.message)) : request.resolve(message.result);
  };
  ws.onclose = () => {
    for (const request of pending.values()) { clearTimeout(request.timer); request.reject(Error('CDP disconnected')); }
    pending.clear();
  };
  function call(method, params = {}) {
    return new Promise((resolve, reject) => {
      if (ws.readyState !== WebSocket.OPEN) { reject(Error('CDP disconnected')); return; }
      const id = ++sequence;
      const timer = setTimeout(() => { pending.delete(id); reject(Error('CDP request timeout')); }, 20000);
      pending.set(id, { resolve, reject, timer });
      try { ws.send(JSON.stringify({ id, method, params })); }
      catch (error) { clearTimeout(timer); pending.delete(id); reject(error); }
    });
  }
  return {
    close: () => ws.close(),
    async evaluate(expression) {
      const result = await call('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
      if (result.exceptionDetails) throw Error(result.exceptionDetails.text + ': ' + (result.exceptionDetails.exception?.description || ''));
      return result.result.value;
    },
  };
}
export async function targets() {
  const response = await fetch('http://127.0.0.1:8080/json/list', { signal: AbortSignal.timeout(3000), redirect: 'error' });
  if (!response.ok) throw Error('CDP HTTP ' + response.status);
  const list = await response.json();
  if (!Array.isArray(list)) throw Error('Lista CDP incompatible');
  return list.filter(t => {
    try { return t.type === 'page' && t.title === 'SharedJSContext' && new URL(t.url).origin === 'https://steamloopback.host'; }
    catch { return false; }
  });
}
