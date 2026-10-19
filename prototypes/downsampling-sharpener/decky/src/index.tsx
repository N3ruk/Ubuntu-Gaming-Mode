import { callable, definePlugin } from "@decky/api";
import { PanelSection, PanelSectionRow, SliderField, ToggleField, staticClasses } from "@decky/ui";
import { useEffect, useState } from "react";
import { FaWandMagicSparkles } from "react-icons/fa6";

type Session = {
  display: string;
  detected: boolean;
  active: boolean;
  enabledApplied: boolean;
  strengthApplied: boolean;
};
type Status = { enabled: boolean; strength: number; sessions: Session[] };

const getStatus = callable<[], Status>("get_status");
const setEnabled = callable<[boolean], Status>("set_enabled");
const setStrength = callable<[number], Status>("set_strength");

function label(status: Status | null): string {
  if (!status || !status.sessions.length) return "Gamescope UGM compatible no detectado";
  const session = status.sessions[0];
  if (!session.enabledApplied || !session.strengthApplied) return "Sincronizando controles…";
  if (!status.enabled) return "Filtro apagado";
  if (session.active) return `Filtro activo (${session.display})`;
  if (session.detected) return "Downsampling detectado; esperando aplicar filtro";
  return "Esperando downsampling en un juego";
}

function Content() {
  const [status, update] = useState<Status | null>(null);
  const [error, setError] = useState("");
  useEffect(() => {
    let mounted = true;
    const refresh = async () => {
      try {
        const next = await getStatus();
        if (mounted) { update(next); setError(""); }
      } catch { if (mounted) setError("No se pudo consultar el filtro"); }
    };
    void refresh();
    const timer = setInterval(() => { void refresh(); }, 2000);
    return () => { mounted = false; clearInterval(timer); };
  }, []);
  const changeEnabled = async (enabled: boolean) => {
    try { update(await setEnabled(enabled)); setError(""); }
    catch { setError("No se pudo cambiar el interruptor"); }
  };
  const changeStrength = async (strength: number) => {
    try { update(await setStrength(Math.round(strength))); setError(""); }
    catch { setError("No se pudo cambiar la intensidad"); }
  };
  return <PanelSection title="Nitidez de downsampling">
    <PanelSectionRow>
      <ToggleField label="Activar automáticamente" checked={status?.enabled ?? false}
        disabled={!status || !status.sessions.length} onChange={changeEnabled} />
    </PanelSectionRow>
    <PanelSectionRow>
      <SliderField label="Intensidad" value={status?.strength ?? 25} min={0} max={100}
        step={5} disabled={!status || !status.sessions.length || !status.enabled}
        onChange={changeStrength} />
    </PanelSectionRow>
    <PanelSectionRow><div style={{fontSize: "12px", opacity: 0.8}}>{error || label(status)}</div></PanelSectionRow>
  </PanelSection>;
}

export default definePlugin(() => ({
  name: "UGM Downsampling Sharpener",
  titleView: <div className={staticClasses.Title}>UGM Downsampling Sharpener</div>,
  content: <Content />,
  icon: <FaWandMagicSparkles />,
  onDismount() {},
}));
