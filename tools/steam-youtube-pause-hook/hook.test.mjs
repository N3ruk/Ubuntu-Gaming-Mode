import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {installPauseHook} from './frontend.mjs';
const contract = JSON.parse(fs.readFileSync(new URL('./contract.json', import.meta.url)));
function harness(patched = true) {
  const calls = [];
  function fresh() {return {destroy() {calls.push('destroy');}, stopVideo() {calls.push('stop');}};}
  function ready(player) {
    Object.assign(player, {pauseVideo() {calls.push('pause');}, playVideo() {calls.push('play');}, setSize(w,h) {calls.push(['size',w,h]);}});
  }
  class Component {
    constructor() {this.props = {video: 'A', forcePause: false, autoplay: true, controls: true}; this.m_player = fresh(); this.m_bPlayerReady = false;}
    CreatePlayer(next) {this.DestroyPlayer(); this.m_player = fresh(); this.createdVideo = next.video; this.m_bPlayerReady = false;}
    OnPlayerReady() {this.m_bPlayerReady = true; if (this.props.autoplay) this.m_player.playVideo(); this.props.onPlayerReady?.();}
    componentWillUnmount() {this.DestroyPlayer();}
  }
  // Replay the actual Steam methods involved in the captured exception.
  for (const name of ['shouldComponentUpdate','componentDidUpdate','DestroyPlayer']) {
    const value = Function('return ({'+contract.methods[name]+'})')()[name];
    Object.defineProperty(Component.prototype, name, {value, configurable:true, writable:true});
  }
  for (const name of ['CreatePlayer','OnPlayerReady']) {
    const original = Component.prototype[name], cache = new WeakMap();
    Object.defineProperty(Component.prototype, name, {configurable:true, get() {if(!cache.has(this))cache.set(this,original.bind(this));return cache.get(this);}});
  }
  const proto = Component.prototype;
  const expected = Object.fromEntries(['shouldComponentUpdate','componentDidUpdate','DestroyPlayer','componentWillUnmount'].map(k=>[k,proto[k].toString()]));
  const originals = Object.getOwnPropertyDescriptors(proto);
  const hook = patched ? installPauseHook(proto, expected) : null;
  const c = new Component();
  function update(next) {const prev=c.props; const render=c.shouldComponentUpdate(next,{}); c.props=next; if(render)c.componentDidUpdate(prev);return render;}
  return {c,hook,calls,ready,update,proto,originals};
}
test('regresión: Steam original falla con instancia todavía sin API',()=>{
 const {c}=harness(false);c.props={...c.props,forcePause:true};assert.throws(()=>c.componentDidUpdate({forcePause:false}),/pauseVideo/);
});
test('mando pausa/reanuda sin render ni recreación; conserva reproductor',()=>{
 const {c,hook,calls,ready,update}=harness();const player=c.m_player;ready(player);c.OnPlayerReady({target:player});calls.length=0;
 assert.equal(update({...c.props,forcePause:true}),false);assert.equal(update({...c.props,forcePause:false}),false);
 assert.equal(c.m_player,player);assert.deepEqual(calls,['pause','play']);assert.equal(hook.status().counts.avoidedRenders,2);
});
test('traza real: recreación, forcePause antes de ready, autoplay y pausa pendiente',()=>{
 const {c,calls,ready}=harness();const old=c.m_player;const next={...c.props,forcePause:true};c.CreatePlayer(next);c.props=next;
 assert.doesNotThrow(()=>c.componentDidUpdate({forcePause:false}));assert.ok(!calls.includes('pause'));
 c.OnPlayerReady({target:old});assert.equal(c.m_bPlayerReady,false);
 ready(c.m_player);c.OnPlayerReady({target:c.m_player});assert.deepEqual(calls.slice(-2),['play','pause']);
});
test('varias pulsaciones durante carga: prevalece la última orden',()=>{
 const {c,calls,ready,update}=harness();update({...c.props,forcePause:true});update({...c.props,forcePause:false});
 ready(c.m_player);c.OnPlayerReady({target:c.m_player});assert.ok(!calls.includes('pause'));assert.equal(calls.at(-1),'play');
});
test('vídeo nuevo recibe su intención, nunca una orden pendiente del anterior',()=>{
 const {c,calls,ready,update}=harness();update({...c.props,forcePause:true});const stale=c.m_player;
 update({...c.props,video:'B',forcePause:false});c.OnPlayerReady({target:stale});
 ready(c.m_player);c.OnPlayerReady({target:c.m_player});assert.equal(c.createdVideo,'B');assert.ok(!calls.includes('pause'));
});
test('ready tardío tras desmontaje no ejecuta controles',()=>{
 const {c,calls,ready,update}=harness();update({...c.props,forcePause:true});const stale=c.m_player;ready(stale);c.componentWillUnmount();const length=calls.length;
 c.OnPlayerReady({target:stale});assert.equal(calls.length,length);
});
test('ratón: un ready sin pausa pendiente no fuerza nuevos controles',()=>{
 const {c,calls,ready}=harness();ready(c.m_player);c.OnPlayerReady({target:c.m_player});calls.length=0;
 c.m_player.pauseVideo();c.componentDidUpdate(c.props);assert.deepEqual(calls,['pause']);
});
test('cambio de tamaño conserva el comportamiento original',()=>{
 const {c,calls,ready,update}=harness();ready(c.m_player);c.OnPlayerReady({target:c.m_player});calls.length=0;
 update({...c.props,width:640,height:360});assert.deepEqual(calls,[['size',640,360]]);
});
test('reversión restaura descriptores y callbacks enlazados ya entregados',()=>{
 const {c,hook,calls,ready,proto,originals}=harness();const callback=c.OnPlayerReady;ready(c.m_player);hook.remove();
 for(const name of ['CreatePlayer','OnPlayerReady','shouldComponentUpdate','componentDidUpdate','DestroyPlayer','componentWillUnmount'])assert.deepEqual(Object.getOwnPropertyDescriptor(proto,name),originals[name]);
 callback({target:c.m_player});assert.deepEqual(calls,['play']);assert.equal(hook.status().active,false);
});
test('contrato distinto se rechaza sin modificar el prototipo',()=>{
 const {proto,originals}=harness(false);assert.throws(()=>installPauseHook(proto,{}),/incompatible/);assert.deepEqual(Object.getOwnPropertyDescriptors(proto),originals);
});
