import { definePlugin } from '@decky/api';
import { Content } from './Content';
const AddIcon = () => <svg width="1em" height="1em" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" aria-hidden="true">
  <rect x="3" y="3" width="18" height="18" rx="3" /><path d="M12 7v10M7 12h10" />
</svg>;
export default definePlugin(() => ({
  name: 'UGM Añadir contenido Non-Steam',
  titleView: <div style={{ fontSize: 14 }}>UGM · Añadir contenido Non-Steam</div>,
  icon: <AddIcon />, content: <Content />,
}));
