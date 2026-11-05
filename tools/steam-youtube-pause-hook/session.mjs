import {connect, targets} from './cdp.mjs';
import {hookExpression} from './runtime.mjs';
const action = process.argv[2] || 'status';
if (!['apply', 'status', 'remove'].includes(action)) throw Error('Uso: session.mjs apply|status|remove');
const all = await targets(); if (all.length !== 1) throw Error('SharedJSContext no único');
const client = await connect(all[0]);
try {
  console.log(JSON.stringify(await client.evaluate(hookExpression(action)), null, 2));
} finally {client.close();}
