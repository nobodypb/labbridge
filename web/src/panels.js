/** @type {Map<string,{id:string,capability:string,render:Function}>} */
export const panels = new Map();
export function registerPanel(definition) {
 if(panels.has(definition.id))throw Error(`Duplicate panel ${definition.id}`);
 panels.set(definition.id,definition);
}
export function panelIds(device){return [...panels].filter(([,p])=>device.capabilities.includes(p.capability)).map(([id])=>id);}
export function canEnable(device,leased){return leased&&device.state==='ready'&&device.output==='off';}
