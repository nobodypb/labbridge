export const panels = new Map([
 ['output',{capability:'output'}],
 ['load',{capability:'load.cc'}],
 ['source',{capability:'source.cc_cv'}],
 ['interlock',{capability:'switch.path'}],
 ['temperature',{capability:'temperature.external'}],
 ['settings',{capability:'dl24m.settings'}],
]);
export function panelIds(device){return [...panels].filter(([,p])=>device.capabilities.includes(p.capability)).map(([id])=>id);}
export function canEnable(device,leased){return leased&&device.state==='ready'&&device.output==='off';}
