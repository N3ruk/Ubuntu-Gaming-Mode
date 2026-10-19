const manifest = {"name":"Sharp Filter Selector"};
const API_VERSION = 2;
const internalAPIConnection = window.__DECKY_SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_deckyLoaderAPIInit;
if (!internalAPIConnection) {
    throw new Error('[@decky/api]: Failed to connect to the loader as as the loader API was not initialized. This is likely a bug in Decky Loader.');
}
let api;
try {
    api = internalAPIConnection.connect(API_VERSION, manifest.name);
}
catch {
    api = internalAPIConnection.connect(1, manifest.name);
    console.warn(`[@decky/api] Requested API version ${API_VERSION} but the running loader only supports version 1. Some features may not work.`);
}
if (api._version != API_VERSION) {
    console.warn(`[@decky/api] Requested API version ${API_VERSION} but the running loader only supports version ${api._version}. Some features may not work.`);
}
const callable = api.callable;
const definePlugin = (fn) => {
    return (...args) => {
        return fn(...args);
    };
};

var DefaultContext = {
  color: undefined,
  size: undefined,
  className: undefined,
  style: undefined,
  attr: undefined
};
var IconContext = SP_REACT.createContext && /*#__PURE__*/SP_REACT.createContext(DefaultContext);

var _excluded = ["attr", "size", "title"];
function _objectWithoutProperties(e, t) { if (null == e) return {}; var o, r, i = _objectWithoutPropertiesLoose(e, t); if (Object.getOwnPropertySymbols) { var n = Object.getOwnPropertySymbols(e); for (r = 0; r < n.length; r++) o = n[r], -1 === t.indexOf(o) && {}.propertyIsEnumerable.call(e, o) && (i[o] = e[o]); } return i; }
function _objectWithoutPropertiesLoose(r, e) { if (null == r) return {}; var t = {}; for (var n in r) if ({}.hasOwnProperty.call(r, n)) { if (-1 !== e.indexOf(n)) continue; t[n] = r[n]; } return t; }
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
function ownKeys(e, r) { var t = Object.keys(e); if (Object.getOwnPropertySymbols) { var o = Object.getOwnPropertySymbols(e); r && (o = o.filter(function (r) { return Object.getOwnPropertyDescriptor(e, r).enumerable; })), t.push.apply(t, o); } return t; }
function _objectSpread(e) { for (var r = 1; r < arguments.length; r++) { var t = null != arguments[r] ? arguments[r] : {}; r % 2 ? ownKeys(Object(t), true).forEach(function (r) { _defineProperty(e, r, t[r]); }) : Object.getOwnPropertyDescriptors ? Object.defineProperties(e, Object.getOwnPropertyDescriptors(t)) : ownKeys(Object(t)).forEach(function (r) { Object.defineProperty(e, r, Object.getOwnPropertyDescriptor(t, r)); }); } return e; }
function _defineProperty(e, r, t) { return (r = _toPropertyKey(r)) in e ? Object.defineProperty(e, r, { value: t, enumerable: true, configurable: true, writable: true }) : e[r] = t, e; }
function _toPropertyKey(t) { var i = _toPrimitive(t, "string"); return "symbol" == typeof i ? i : i + ""; }
function _toPrimitive(t, r) { if ("object" != typeof t || !t) return t; var e = t[Symbol.toPrimitive]; if (void 0 !== e) { var i = e.call(t, r); if ("object" != typeof i) return i; throw new TypeError("@@toPrimitive must return a primitive value."); } return ("string" === r ? String : Number)(t); }
function Tree2Element(tree) {
  return tree && tree.map((node, i) => /*#__PURE__*/SP_REACT.createElement(node.tag, _objectSpread({
    key: i
  }, node.attr), Tree2Element(node.child)));
}
function GenIcon(data) {
  return props => /*#__PURE__*/SP_REACT.createElement(IconBase, _extends({
    attr: _objectSpread({}, data.attr)
  }, props), Tree2Element(data.child));
}
function IconBase(props) {
  var elem = conf => {
    var attr = props.attr,
      size = props.size,
      title = props.title,
      svgProps = _objectWithoutProperties(props, _excluded);
    var computedSize = size || conf.size || "1em";
    var className;
    if (conf.className) className = conf.className;
    if (props.className) className = (className ? className + " " : "") + props.className;
    return /*#__PURE__*/SP_REACT.createElement("svg", _extends({
      stroke: "currentColor",
      fill: "currentColor",
      strokeWidth: "0"
    }, conf.attr, attr, svgProps, {
      className: className,
      style: _objectSpread(_objectSpread({
        color: props.color || conf.color
      }, conf.style), props.style),
      height: computedSize,
      width: computedSize,
      xmlns: "http://www.w3.org/2000/svg"
    }), title && /*#__PURE__*/SP_REACT.createElement("title", null, title), props.children);
  };
  return IconContext !== undefined ? /*#__PURE__*/SP_REACT.createElement(IconContext.Consumer, null, conf => elem(conf)) : elem(DefaultContext);
}

// THIS FILE IS AUTO GENERATED
function FaWandMagicSparkles (props) {
  return GenIcon({"attr":{"viewBox":"0 0 576 512"},"child":[{"tag":"path","attr":{"d":"M234.7 42.7L197 56.8c-3 1.1-5 4-5 7.2s2 6.1 5 7.2l37.7 14.1L248.8 123c1.1 3 4 5 7.2 5s6.1-2 7.2-5l14.1-37.7L315 71.2c3-1.1 5-4 5-7.2s-2-6.1-5-7.2L277.3 42.7 263.2 5c-1.1-3-4-5-7.2-5s-6.1 2-7.2 5L234.7 42.7zM46.1 395.4c-18.7 18.7-18.7 49.1 0 67.9l34.6 34.6c18.7 18.7 49.1 18.7 67.9 0L529.9 116.5c18.7-18.7 18.7-49.1 0-67.9L495.3 14.1c-18.7-18.7-49.1-18.7-67.9 0L46.1 395.4zM484.6 82.6l-105 105-23.3-23.3 105-105 23.3 23.3zM7.5 117.2C3 118.9 0 123.2 0 128s3 9.1 7.5 10.8L64 160l21.2 56.5c1.7 4.5 6 7.5 10.8 7.5s9.1-3 10.8-7.5L128 160l56.5-21.2c4.5-1.7 7.5-6 7.5-10.8s-3-9.1-7.5-10.8L128 96 106.8 39.5C105.1 35 100.8 32 96 32s-9.1 3-10.8 7.5L64 96 7.5 117.2zm352 256c-4.5 1.7-7.5 6-7.5 10.8s3 9.1 7.5 10.8L416 416l21.2 56.5c1.7 4.5 6 7.5 10.8 7.5s9.1-3 10.8-7.5L480 416l56.5-21.2c4.5-1.7 7.5-6 7.5-10.8s-3-9.1-7.5-10.8L480 352l-21.2-56.5c-1.7-4.5-6-7.5-10.8-7.5s-9.1 3-10.8 7.5L416 352l-56.5 21.2z"},"child":[]}]})(props);
}

const setNisEnabled = callable("set_nis_enabled");
const setFsrOverride = callable("set_fsr_override");
const setNisSharpness = callable("set_nis_sharpness");
const getStatus = callable("get_status");
const getCapabilities = callable("get_capabilities");
const NIS_ENABLED_KEY = "sharp-filter-nis-enabled";
const NIS_SHARPNESS_KEY = "sharp-filter-nis-sharpness";
const FSR_OVERRIDE_KEY = "sharp-filter-fsr-override";
function storedNisEnabled() {
    return localStorage.getItem(NIS_ENABLED_KEY) === "true";
}
function storedFsrOverride() {
    return localStorage.getItem(FSR_OVERRIDE_KEY) === "true";
}
function storedSharpness() {
    const value = Number(localStorage.getItem(NIS_SHARPNESS_KEY));
    return Number.isInteger(value) && value >= 0 && value <= 5 ? value : 5;
}
function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}
function targetSummary(result) {
    const diagnostics = Array.isArray(result.diagnostics) ? result.diagnostics : [];
    if (!diagnostics.length)
        return "";
    return diagnostics
        .map((d) => {
        const display = d.display ?? "?";
        const sid = Number.isInteger(d.serverId) ? `#${d.serverId}` : "";
        const legacy = d.legacyFilter ?? "?";
        const modern = d.newFilter ?? "?";
        const feedback = d.fsrFeedback === 1 ? " FSR" : "";
        return `${display}${sid}[${legacy}/${modern}]${feedback}`;
    })
        .join(" · ");
}
function nativeLabel(capabilities) {
    if (!capabilities?.supportsSGSR)
        return "FSR";
    return capabilities.hdrInput ? "FSR (HDR fallback)" : "SGSR";
}
function Content() {
    const [nisEnabled, setNisEnabledState] = SP_REACT.useState(storedNisEnabled);
    const [fsrOverride, setFsrOverrideState] = SP_REACT.useState(storedFsrOverride);
    const [sharpness, setSharpnessState] = SP_REACT.useState(storedSharpness);
    const [capabilities, setCapabilities] = SP_REACT.useState(null);
    const [status, setStatus] = SP_REACT.useState("Checking Gamescope…");
    const clearHdrFeedback = () => setCapabilities((previous) => previous ? {
        ...previous,
        hdrInput: false,
        hdrInputKnown: false,
        hdrSource: "unknown",
        showFsrOverride: Boolean(previous.supportsSGSR),
    } : previous);
    const describe = (nis, fsr, level, displays, caps = capabilities) => {
        const targets = displays?.length ? ` (${displays.join(", ")})` : "";
        if (nis)
            return `NIS active · sharpness ${level}/5${targets}`;
        if (fsr)
            return `FSR override active · sharpness ${level}/5${targets}`;
        return `Sharp native · ${nativeLabel(caps)}${targets}`;
    };
    const refreshCapabilities = async () => {
        try {
            const result = await getCapabilities();
            setCapabilities(result);
            return result;
        }
        catch {
            clearHdrFeedback();
            return null;
        }
    };
    const verifyNis = async (level, fallbackDisplays) => {
        for (let attempt = 0; attempt < 5; attempt++) {
            await sleep(attempt === 0 ? 350 : 500);
            const result = await getStatus();
            if (result?.success && result.enabled) {
                const targets = targetSummary(result);
                setStatus(targets
                    ? `NIS verified active · ${targets}`
                    : `NIS verified active · sharpness ${level}/5`);
                return true;
            }
        }
        try {
            const result = await getStatus();
            if (result?.success) {
                const targets = targetSummary(result);
                if (result.requested && result.enforcing) {
                    setStatus(targets
                        ? `NIS failed · targets ${targets}`
                        : "NIS failed · Gamescope targets found but NIS is not active");
                }
                else {
                    setStatus(describe(Boolean(result.enabled), Boolean(result.fsrOverride), Number.isInteger(result.level) ? Number(result.level) : level, result.displays ?? fallbackDisplays));
                }
            }
            else {
                setStatus(result?.error || "Could not verify the active Gamescope filter");
            }
        }
        catch {
            setStatus("NIS requested · verification unavailable");
        }
        return false;
    };
    const applyNis = async (enabled) => {
        setStatus(enabled ? "Applying NIS…" : "Restoring native Sharp filter…");
        try {
            const engineResult = await setNisEnabled(enabled);
            if (!engineResult.success) {
                setStatus(engineResult.error || "Gamescope is not available");
                return;
            }
            let displays = engineResult.displays;
            if (enabled) {
                const sharpnessResult = await setNisSharpness(sharpness);
                if (!sharpnessResult.success) {
                    setStatus(sharpnessResult.error || "Could not apply NIS sharpness");
                    return;
                }
                displays = sharpnessResult.displays;
                localStorage.setItem(FSR_OVERRIDE_KEY, "false");
                setFsrOverrideState(false);
            }
            localStorage.setItem(NIS_ENABLED_KEY, String(enabled));
            setNisEnabledState(enabled);
            const caps = await refreshCapabilities();
            if (enabled) {
                setStatus("NIS requested · verifying…");
                await verifyNis(sharpness, displays);
            }
            else {
                setStatus(describe(false, false, sharpness, displays, caps ?? capabilities));
            }
        }
        catch (error) {
            setStatus(`Error: ${String(error)}`);
        }
    };
    const applyFsr = async (enabled) => {
        setStatus(enabled ? "Applying FSR override…" : "Restoring native SGSR…");
        try {
            const result = await setFsrOverride(enabled);
            if (!result.success) {
                setStatus(result.error || "Could not change the FSR override");
                return;
            }
            let displays = result.displays;
            if (enabled) {
                const sharpnessResult = await setNisSharpness(sharpness);
                if (!sharpnessResult.success) {
                    setStatus(sharpnessResult.error || "Could not apply FSR sharpness");
                    return;
                }
                displays = sharpnessResult.displays;
                localStorage.setItem(NIS_ENABLED_KEY, "false");
                setNisEnabledState(false);
            }
            localStorage.setItem(FSR_OVERRIDE_KEY, String(enabled));
            setFsrOverrideState(enabled);
            const caps = await refreshCapabilities();
            setStatus(describe(false, enabled, sharpness, displays, caps ?? capabilities));
        }
        catch (error) {
            setStatus(`Error: ${String(error)}`);
        }
    };
    const applySharpness = async (value) => {
        const level = Math.max(0, Math.min(5, Math.round(value)));
        setSharpnessState(level);
        localStorage.setItem(NIS_SHARPNESS_KEY, String(level));
        if (!nisEnabled && !fsrOverride)
            return;
        setStatus(nisEnabled ? "Applying NIS sharpness…" : "Applying FSR sharpness…");
        try {
            const result = await setNisSharpness(level);
            if (!result.success) {
                setStatus(result.error || "Could not apply filter sharpness");
                return;
            }
            if (nisEnabled) {
                setStatus(`NIS requested · sharpness ${level}/5 · verifying…`);
                await verifyNis(level, result.displays);
            }
            else {
                setStatus(describe(false, true, level, result.displays));
            }
        }
        catch (error) {
            setStatus(`Error: ${String(error)}`);
        }
    };
    SP_REACT.useEffect(() => {
        let mounted = true;
        let timer;
        const initialize = async () => {
            try {
                const caps = await getCapabilities();
                if (!mounted)
                    return;
                setCapabilities(caps);
                const desiredNis = storedNisEnabled();
                let desiredFsr = storedFsrOverride();
                const desiredSharpness = storedSharpness();
                if (caps.success && !caps.supportsSGSR && desiredFsr) {
                    desiredFsr = false;
                    localStorage.setItem(FSR_OVERRIDE_KEY, "false");
                }
                if (desiredNis) {
                    const engineResult = await setNisEnabled(true);
                    if (!mounted)
                        return;
                    if (!engineResult.success) {
                        setStatus(engineResult.error || "Available in a Gamescope session");
                        return;
                    }
                    const sharpnessResult = await setNisSharpness(desiredSharpness);
                    if (!mounted)
                        return;
                    if (!sharpnessResult.success) {
                        setStatus(sharpnessResult.error || "Could not apply NIS sharpness");
                        return;
                    }
                    localStorage.setItem(FSR_OVERRIDE_KEY, "false");
                    setFsrOverrideState(false);
                    setNisEnabledState(true);
                    setSharpnessState(desiredSharpness);
                    setStatus("NIS requested · verifying…");
                    await verifyNis(desiredSharpness, sharpnessResult.displays);
                }
                else if (desiredFsr && caps.supportsSGSR) {
                    const result = await setFsrOverride(true);
                    if (!mounted)
                        return;
                    if (!result.success) {
                        setStatus(result.error || "Could not restore FSR override");
                        return;
                    }
                    const sharpnessResult = await setNisSharpness(desiredSharpness);
                    if (!mounted)
                        return;
                    if (!sharpnessResult.success) {
                        setStatus(sharpnessResult.error || "Could not apply FSR sharpness");
                        return;
                    }
                    setNisEnabledState(false);
                    setFsrOverrideState(true);
                    setSharpnessState(desiredSharpness);
                    setStatus(describe(false, true, desiredSharpness, sharpnessResult.displays ?? result.displays, caps));
                }
                else if (caps.success) {
                    // Passive initialization must never overwrite the filter or scaler
                    // currently selected by Steam/QAM.
                    setNisEnabledState(false);
                    setFsrOverrideState(false);
                    setSharpnessState(desiredSharpness);
                    setStatus(describe(false, false, desiredSharpness, undefined, caps));
                }
                else {
                    setNisEnabledState(false);
                    setFsrOverrideState(false);
                    setSharpnessState(desiredSharpness);
                    setStatus("Gamescope capability detection unavailable");
                }
                timer = setInterval(async () => {
                    try {
                        const next = await getCapabilities();
                        if (mounted)
                            setCapabilities(next);
                    }
                    catch {
                        // Keep SGSR capability, but do not hide FSR using stale HDR feedback.
                        if (mounted)
                            clearHdrFeedback();
                    }
                }, 1500);
            }
            catch (error) {
                if (!mounted)
                    return;
                setStatus(`Error: ${String(error)}`);
            }
        };
        void initialize();
        return () => {
            mounted = false;
            if (timer !== undefined)
                clearInterval(timer);
        };
    }, []);
    const showFsrOverride = Boolean(capabilities?.showFsrOverride);
    const supportsSgsr = Boolean(capabilities?.supportsSGSR);
    return (SP_JSX.jsxs(DFL.PanelSection, { title: "Sharp Filter", children: [SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.ToggleField, { label: "Use NIS", description: nisEnabled
                        ? "Override the native Sharp filter with NVIDIA Image Scaling."
                        : `Native Sharp filter: ${nativeLabel(capabilities)}.`, checked: nisEnabled, onChange: applyNis }) }), showFsrOverride && (SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.ToggleField, { label: "Use FSR", description: fsrOverride
                        ? "Override SGSR with AMD FidelityFX Super Resolution."
                        : "Leave this off to use Gamescope's native SGSR Sharp filter.", checked: fsrOverride, onChange: applyFsr }) })), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.SliderField, { label: `Sharpness (${sharpness}/5)`, description: "Shared FSR/NIS sharpening: 0 minimum, 5 maximum. Disabled when using native Sharp/SGSR.", value: sharpness, min: 0, max: 5, step: 1, notchCount: 6, disabled: !nisEnabled && !fsrOverride, onChange: applySharpness }) }), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsxs("div", { style: { fontSize: "12px", opacity: 0.8, padding: "4px 0 8px" }, children: [status, supportsSgsr && capabilities?.gamescopeVersion
                            ? ` · Gamescope ${capabilities.gamescopeVersion}`
                            : ""] }) })] }));
}
var index = definePlugin(() => ({
    name: "Sharp Filter Selector",
    titleView: SP_JSX.jsx("div", { className: DFL.staticClasses.Title, children: "Sharp Filter Selector" }),
    content: SP_JSX.jsx(Content, {}),
    icon: SP_JSX.jsx(FaWandMagicSparkles, {}),
    onDismount() { },
}));

export { index as default };
//# sourceMappingURL=index.js.map
