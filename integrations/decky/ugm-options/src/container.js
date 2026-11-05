/* Decky host supplies SP_REACT/SP_JSX/DFL. No duplicate React or global patch. */
const masterConnection = window.__DECKY_SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_deckyLoaderAPIInit;
let masterAPI;
try { masterAPI = masterConnection.connect(2, 'UGM Options'); }
catch { masterAPI = masterConnection.connect(1, 'UGM Options'); }
const moduleCall = masterAPI.callable('module_call');
const moduleStatus = masterAPI.callable('module_status');
const cache = new Map();
const savedErrors = new Map();

function openModule(id) {
  if (savedErrors.has(id)) throw savedErrors.get(id);
  if (!cache.has(id)) {
    try { cache.set(id, moduleFactories[id]()); }
    catch (error) { savedErrors.set(id, error); throw error; }
  }
  return cache.get(id);
}

class ModuleBoundary extends SP_REACT.Component {
  constructor(props) { super(props); this.state = { error: null }; }
  static getDerivedStateFromError(error) { return { error }; }
  render() {
    if (this.state.error) return SP_JSX.jsx(DFL.PanelSection, { title: 'Módulo no disponible',
      children: SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx('div', {
        role: 'alert', children: String(this.state.error.message || this.state.error) }) }) });
    return this.props.children;
  }
}

function ModulePanel({ id }) {
  const [status, setStatus] = SP_REACT.useState(null);
  const [error, setError] = SP_REACT.useState('');
  SP_REACT.useEffect(() => {
    let active = true;
    moduleStatus().then(value => {
      if (active) setStatus(value[id] || { ready: false, error: 'Módulo desconocido' });
    }).catch(() => { if (active) setError('No se pudo consultar UGM Options.'); });
    return () => { active = false; };
  }, [id]);
  if (error || (status && !status.ready)) return SP_JSX.jsx(DFL.PanelSection, {
    title: 'Módulo no disponible', children: SP_JSX.jsx(DFL.PanelSectionRow, {
      children: SP_JSX.jsx('div', { role: 'alert', children: error || status.error }) }) });
  if (!status) return SP_JSX.jsx(DFL.PanelSection, { title: 'Cargando…', spinner: true });
  // Return the exact original content element; no controls/engine rewritten.
  return openModule(id).content;
}

function Options() {
  const [selected, select] = SP_REACT.useState(null);
  const refs = SP_REACT.useRef({});
  const focusReturn = SP_REACT.useRef(null);
  SP_REACT.useEffect(() => {
    if (selected === null && focusReturn.current) {
      refs.current[focusReturn.current]?.focus();
      focusReturn.current = null;
    }
  }, [selected]);
  const back = () => { focusReturn.current = selected; select(null); };
  if (selected) return SP_JSX.jsxs(DFL.Focusable, { onCancel: event => { event.stopPropagation(); back(); }, children: [
    SP_JSX.jsx(DFL.PanelSection, { children: SP_JSX.jsx(DFL.PanelSectionRow, {
      children: SP_JSX.jsx(DFL.ButtonItem, { layout: 'below', onClick: back,
        children: '‹ UGM Options' }) }) }),
    SP_JSX.jsx(ModuleBoundary, { children: SP_JSX.jsx(ModulePanel, { id: selected }) }, selected),
  ] });
  return SP_JSX.jsx(DFL.PanelSection, { title: 'UGM Options', children:
    moduleRegistry.map(module => SP_JSX.jsx(DFL.PanelSectionRow, {
      children: SP_JSX.jsx(DFL.ButtonItem, { layout: 'below',
        ref: node => { refs.current[module.id] = node; },
        onClick: () => select(module.id), children: module.label }) }, module.id)) });
}

const OptionsIcon = () => SP_JSX.jsxs('svg', { width: '1em', height: '1em',
  viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', strokeWidth: 1.8,
  'aria-hidden': true, children: [
    SP_JSX.jsx('rect', { x: 3, y: 3, width: 7, height: 7, rx: 1.5 }),
    SP_JSX.jsx('rect', { x: 14, y: 3, width: 7, height: 7, rx: 1.5 }),
    SP_JSX.jsx('rect', { x: 3, y: 14, width: 7, height: 7, rx: 1.5 }),
    SP_JSX.jsx('rect', { x: 14, y: 14, width: 7, height: 7, rx: 1.5 }),
  ] });

const index = () => ({
  name: 'UGM Options', titleView: SP_JSX.jsx('div', { children: 'UGM Options' }),
  content: SP_JSX.jsx(Options, {}), icon: SP_JSX.jsx(OptionsIcon, {}),
  onDismount() {
    for (const plugin of cache.values()) {
      try { plugin.onDismount?.(); } catch (error) { console.error('UGM module cleanup', error); }
    }
    cache.clear(); savedErrors.clear();
  },
});
export { index as default };
