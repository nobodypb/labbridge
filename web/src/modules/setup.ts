import {rdWifiSetup} from './rd-wifi';
import type {SetupContext} from './rd-wifi';
// Node-level setup modules register here; device control panels remain separate.
const modules=[rdWifiSetup];
export function renderSetupPanels(context:SetupContext){for(const module of modules)module.render(context);}
