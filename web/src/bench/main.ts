// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import type {Dl24mSettings,Measurements} from '../../generated/src/index';
import {BenchAdapter,message} from './adapter';
import {features,settingsFields} from './features';
import {BenchSession} from './session.js';
import {CanvasChart} from './chart.js';
const $=<T extends HTMLElement=HTMLElement>(id:string)=>document.getElementById(id) as T;
const input=(id:string)=>$<HTMLInputElement>(id);
const button=(id:string)=>$<HTMLButtonElement>(id);
const api=new BenchAdapter();
const dirtySettings=new Set<string>();
let session=new BenchSession(3000),sessionIdentity='',pollMode='live',recordingGap=false,localPending=false,request=0;
const chart=new CanvasChart($<HTMLCanvasElement>('chart'),{maxpts:900,css:(v:string)=>getComputedStyle(document.documentElement).getPropertyValue(v).trim(),series:[{key:'v',cssVar:'--v'},{key:'i',cssVar:'--i'},{key:'w',cssVar:'--w'}]});
function report(text:string,error=false){$('benchStatus').textContent=text;$('benchStatus').classList.toggle('error',error);}
async function run(fn:()=>Promise<unknown>,priority=false){
 if((localPending||api.auth.loggingOut)&&!priority)return;
 const n=++request;localPending=true;render();
 try{const result=await fn();if(n===request&&result&&typeof result==='object'&&'status'in result)report(`Operation ${result.status}`);}
 catch(e){if(n===request)report(await message(e),true);}
 finally{if(n===request)localPending=false;render();}
}
function bind(id:string,fn:()=>Promise<unknown>,priority=false){button(id).onclick=()=>void run(fn,priority);}
function store(key:string,value?:string){try{if(value!==undefined)localStorage.setItem('bench:'+key,value);return localStorage.getItem('bench:'+key);}catch{return null;}}
function setTheme(theme:string){document.documentElement.dataset.theme=theme;store('theme',theme);document.querySelectorAll<HTMLElement>('[data-th]').forEach(b=>b.classList.toggle('sel',b.dataset.th===theme));chart.redraw();}
function number(id:string,min:number,max:number,step:number){
 const el=input(id),n=Number(el.value);
 if(el.value.trim()===''||!Number.isFinite(n)||n<min||n>max||Math.abs((n-min)/step-Math.round((n-min)/step))>1e-6)throw Error(`Enter a valid value for ${el.getAttribute('aria-label')||id}: ${min}–${max}, step ${step}`);
 return n;
}
function targetRange(){const limits=api.device?.limits;return api.mode==='cv'?{min:.5,max:limits?.voltageV??0,step:.01,unit:'V'}:api.mode==='cr'?{min:.1,max:240,step:.1,unit:'Ω'}:api.mode==='cp'?{min:.1,max:limits?.powerW??0,step:.1,unit:'W'}:{min:0,max:limits?.currentA??0,step:.001,unit:'A'};}
function field(id:string){const f=settingsFields[id];return number(id,f.min,f.max,f.step);}
function disable(id:string,disabled:boolean){const e=$(id);if(e&&'disabled'in e)(e as HTMLButtonElement).disabled=disabled;}
function val(id:string,value:number|undefined,decimals=3){$(id).textContent=Number.isFinite(value)?value!.toFixed(decimals):'—';}
function temperature(m:Measurements,kind:string){const t=m.temperatureChannels.find(c=>c.kind===kind&&c.quality==='valid'&&c.ageMs<=(api.device?.limits?.maxAgeMs??0));return t?.valueC;}
function instrumentTemperature(m:Measurements){
 const fine=m.nativeReadings?.find(r=>r.id==='temperature_magnitude'&&r.quality==='valid'&&r.ageMs!==undefined&&r.ageMs<=(api.device?.limits?.maxAgeMs??0)&&Number.isFinite(r.value));
 return fine?{value:fine.value,decimals:2}:{value:temperature(m,'internal'),decimals:1};
}
function elapsed(ms:number){const sec=Math.floor(ms/1000);return [Math.floor(sec/3600),Math.floor(sec/60)%60,sec%60].map(v=>String(v).padStart(2,'0')).join(':');}
function render(){
 const pending=localPending||api.auth.loggingOut,ready=api.writable&&!pending,off=api.off&&!pending;
 $('connLbl').textContent=api.connected?'Connected':'Connect';$('connect').classList.toggle('on',api.connected);$('connect').classList.toggle('off',!api.connected);
 disable('connect',pending||api.busy||!api.auth.state.authenticated);
 const select=$<HTMLSelectElement>('deviceSelect');
 if(Array.from(select.options).map(o=>o.value).join(',')!==api.devices.map(d=>d.id).join(',')){
  select.replaceChildren(...api.devices.map(d=>{const o=document.createElement('option');o.value=d.id;o.textContent=`${d.id} · ${d.identity}`;return o;}));
 }
 select.value=api.device?.id??'';select.disabled=pending||api.busy||!api.connected;
 $('reserve').textContent=api.lease?'Release and stop':'Reserve device';disable('reserve',pending||api.busy||!api.connected||!api.fresh||api.uncertain&&!api.lease);
 disable('stop',!api.device||!api.auth.state.authenticated);disable('setBtn',!ready||!api.supports('current')||!api.modeSupported||api.settings.mode!==0&&!off);
 const range=targetRange();input('setVal').max=String(range.max);input('setVal').min=String(range.min);input('setVal').step=String(range.step);$('setUnit').textContent=`(${range.unit})`;
 disable('loadBtn',api.actor?.output==='on'?!api.auth.state.authenticated:!api.canOn||pending||!api.supports('output'));
 $('loadBtn').classList.toggle('running',api.actor?.output==='on');$('loadBtn').classList.toggle('stopped',api.actor?.output==='off');
 $('loadBtn').querySelector('.lbl')!.textContent=api.busy?'LOAD · confirming…':`LOAD ${api.actor?.output?.toUpperCase()??'UNKNOWN'}`;
 disable('timeBtn',!off||!api.supports('duration'));disable('limBtn',!off);disable('zeroData',!off);
 for(const f of Object.values(settingsFields))if(f.button)disable(f.button,!off);
 document.querySelectorAll<HTMLElement>('[data-mode]').forEach(b=>{b.classList.toggle('sel',api.mode===b.dataset.mode);disable(b.id,!off||!api.device?.features?.some(f=>f.id===b.dataset.mode&&f.status==='supported'));});
 for(const [id,f] of Object.entries(features)){
  const state=f.capability?api.device?.features?.find(s=>s.id===f.capability):undefined;
  const reason=f.capability?(state?.status==='supported'?'':state?.reason??'Connect to discover backend support.'):f.reason;
  if(f.capability||f.reason){disable(id,!!reason||!api.connected||id==='snapBtn'&&(pending||api.busy)||id.startsWith('mode-')&&!off);$(id).title=reason??state?.reason??'';$(id).dataset.help=$(id).title;}
 }
 $('unsupported').textContent='Backend feature status: '+(api.device?.features?.filter(f=>f.status!=='supported').map(f=>`${f.id}: ${f.status} — ${f.reason}`).join(' · ')||'Connect to discover supported and unverified functions.');
 for(const note of Array.from(document.querySelectorAll<HTMLElement>('[data-feature-note]'))){
  const f=features[note.dataset.featureNote!];note.textContent=api.device?.features?.find(s=>s.id===f.capability)?.reason??f.reason??'Connect to discover backend support.';
 }
 disable('export',session.rows.length===0);
 $('setConn').textContent=api.connected?`${api.device?.id} · ${api.actor?.output??'unknown'}${api.lease?' · reserved':''}`:'device not connected';
 for(const [id,f] of Object.entries(settingsFields)){
  const e=input(id),v=api.settings[f.key];
  if(e.tagName==='SELECT'){if(!dirtySettings.has(id))e.value=v===undefined?'':String(v);}
  else e.placeholder=v===undefined?'—':String(v);
 }
 if(api.settings.timeLimitS!==undefined){input('thr').placeholder=String(Math.floor(api.settings.timeLimitS/3600));input('tmin').placeholder=String(Math.floor(api.settings.timeLimitS%3600/60));}
 if(api.measurement?.loadTarget&&api.measurement.loadTarget.mode===api.mode)input('setVal').placeholder=String(api.measurement.loadTarget.value);else if(api.actor?.settings.currentA!==undefined)input('setVal').placeholder=String(api.actor.settings.currentA);
 val('rCut',api.settings.cutoffVoltageV,1);
 $('dataQuality').textContent=!api.connected?'Disconnected — readings unavailable':!api.fresh?'Stale — control locked':pollMode==='load'&&api.actor?.output!=='on'?'Load recording paused · state monitoring active':'Live · fresh measurements';
 const native=api.connected?api.measurement?.nativeReadings??[]:[];
 $('nativeReadings').textContent=native.length?'Native device readings · '+native.map(r=>{
  const fresh=r.quality==='valid'&&r.ageMs!==undefined&&r.ageMs+performance.now()-api.receivedAt<=(api.device?.limits?.maxAgeMs??0);
  return `${r.id}: ${fresh?r.value:'—'} ${r.unit} [${r.origin}; ${fresh?'valid':r.quality==='unavailable'?'unavailable':'stale'}]`;
 }).join(' · '):'Native device readings: —';
 if(!api.fresh){for(const id of ['rV','rI','rW','rR','rT','rExt'])$(id).textContent='—';}
 $('recordingStatus').textContent=[session.full?'Recording paused: 100,000 samples reached. Export CSV, then Clear to record again.':'',recordingGap?'Stream gap: recording interrupted; integration resumes with fresh consecutive samples.':''].filter(Boolean).join(' ');
}
api.onChange=render;
api.onGap=()=>{recordingGap=true;session.break();chart.setPoints(session.points);render();};
api.onError=text=>{session.break();chart.setPoints(session.points);report(text,true);};
api.onSample=m=>{
 const identity=JSON.stringify([m.deviceId,m.bootId]);
 if(identity!==sessionIdentity){recordingGap=false;previousRegisters.clear();diagnosticExport=undefined;$('snapState').textContent='No browser snapshot captured for this device/boot.';$('snapDiff').textContent='';$('rawLive').textContent=$('rawSet').textContent='Capture passive diagnostics for the selected device.';dirtySettings.clear();for(const id of Object.keys(settingsFields))if(input(id).tagName!=='SELECT')input(id).value='';input('setVal').value='';input('thr').value='';input('tmin').value='';session=new BenchSession(api.device?.limits?.maxAgeMs??3000);sessionIdentity=identity;chart.setPoints([]);}
 const good=api.fresh;
 val('rV',good?m.voltageV:undefined);val('rI',good?m.currentA:undefined,4);val('rW',good?m.powerW:undefined);
 val('rR',good&&m.currentA!==undefined&&m.currentA!==0&&m.voltageV!==undefined?m.voltageV/m.currentA:undefined);
 const instrument=instrumentTemperature(m);val('rT',instrument.value,instrument.decimals);val('rExt',temperature(m,'cell'),1);
 if(pollMode==='load'&&api.actor?.output!=='on'){if(session.previous)session.break();return;}
 session.add({...m,internalTemperatureC:instrumentTemperature(m).value,externalTemperatureC:temperature(m,'cell')});
 chart.setPoints(session.points);val('rCap',session.chargeAh*1000);val('rWh',session.energyWh);$('rTime').textContent=elapsed(session.elapsedMs);
};
for(const el of Array.from(document.querySelectorAll<HTMLElement>('[data-mode]')))el.id='mode-'+el.dataset.mode;
for(const [id,f] of Object.entries(features)){
 const el=$(id);if(!el)throw Error(`Missing Bench feature ${id}`);
 if(f.reason){el.title=f.reason;el.dataset.help=f.reason;disable(id,true);}
}
for(const id of ['calV','zeroNoLoad','restoreDefaults','snapBtn','mode-cv']){
 const note=document.createElement('p');note.className='hint';note.dataset.unavailable='true';note.dataset.featureNote=id;note.textContent=features[id].reason??'Connect to discover backend support.';
 const parent=id.startsWith('mode-')?$(id).closest('.grp')!:$(id).closest('.sgrp')??$(id).parentElement!;
 parent.append(note);
}
$('rawLive').textContent=$('rawSet').textContent='Passive BLE diagnostics: use Snapshot to read the bounded capture and cached registers.';
$('snapState').textContent='No browser snapshot captured.';
let previousRegisters=new Map<string,number|undefined>();
let diagnosticExport:unknown;
bind('snapBtn',async()=>{
 if(!api.connected||!api.device||api.device.features?.find(f=>f.id==='diagnostics')?.status!=='supported')throw Error('Passive diagnostics are unavailable');
 const d=await api.dl.getDl24mDiagnostics({deviceId:api.device.id});
 const changes=d.registers.filter(r=>previousRegisters.has(r.name)&&previousRegisters.get(r.name)!==r.value);
 $('rawLive').textContent=d.entries.map(e=>`${e.sequence} ${e.receivedMs}ms gen ${e.generation} ${e.direction.toUpperCase()} ${e.hex}${e.truncated?' [truncated]':''}`).join('\n');
 $('rawSet').textContent=d.registers.map(r=>`${r.name} [${r.command.toString(16)}] ${r.value??'—'} · ${r.quality} · ${r.ageMs??'—'}ms`).join('\n');
 $('snapState').textContent=`${input('snapLabel').value||'Snapshot'} · ${d.entries.length} BLE chunks · ${d.dropped} overwritten · `+(previousRegisters.size?`${changes.length} changed: ${changes.map(r=>r.name).join(', ')||'none'}`:'first snapshot');
 $('snapDiff').textContent=changes.map(r=>`${r.name}: ${previousRegisters.get(r.name)??'—'} → ${r.value??'—'} (${r.quality})`).join('\n');
 previousRegisters=new Map(d.registers.map(r=>[r.name,r.value]));diagnosticExport={boot_id:api.boot,device_id:api.device.id,label:input('snapLabel').value,diagnostics:d};
});
$('snapClear').onclick=()=>{previousRegisters.clear();diagnosticExport=undefined;$('snapState').textContent='Browser snapshots cleared; instrument unchanged.';$('snapDiff').textContent='';};
const diagnosticsExport=document.createElement('button');diagnosticsExport.textContent='Export BLE diagnostics';diagnosticsExport.type='button';$('snapClear').after(diagnosticsExport);
diagnosticsExport.onclick=()=>{if(!diagnosticExport)return;const url=URL.createObjectURL(new Blob([JSON.stringify(diagnosticExport,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='dl24m-ble-diagnostics.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
for(const [id,f] of Object.entries(settingsFields)){
 const e=input(id);e.addEventListener('input',()=>dirtySettings.add(id));e.addEventListener('change',()=>dirtySettings.add(id));e.min=String(f.min);e.max=String(f.max);e.step=String(f.step);
 if(f.button)bind(f.button,async()=>{const result=await api.settingsWrite({[f.key]:field(id)});dirtySettings.delete(id);return result;});
}
// Associate original visual labels with their controls for keyboard/screen-reader use.
for(const row of Array.from(document.querySelectorAll<HTMLElement>('.srow,.setrow>div'))){
 const label=row.querySelector('label'),control=row.querySelector<HTMLInputElement>('input,select');
 if(label&&control){label.htmlFor=control.id;control.setAttribute('aria-label',label.textContent?.trim()??control.id);}
}
const native=document.createElement('div');native.id='nativeReadings';native.className='hint';native.textContent='Native device readings: —';$('benchStatus').after(native);
const quality=document.createElement('div');quality.id='dataQuality';quality.className='hint';$('benchStatus').after(quality);
const recording=document.createElement('div');recording.id='recordingStatus';recording.className='hint';quality.after(recording);
input('thr').max='24';
$('connect').dataset.help='Connect to this LabBridge node using the shared password session. No USB browser access is required.';
$('pollRate').closest('label')!.dataset.help='Push cached measurements at this interval over WebSocket; this does not change device polling.';
$('pollRate').closest('label')!.title='WebSocket measurement push interval';$('pollMode').dataset.help='Live records fresh samples continuously. Load records only while confirmed ON; state monitoring and lease heartbeat continue.';
$('pollMode').title=$('pollMode').dataset.help!;
$('rT').closest('.cell')!.setAttribute('data-help','Instrument temperature magnitude: native numeric replies omit the sign. Register 36 has 0.01 °C encoding; resolution is not accuracy. Never a cell-temperature channel.');
$('rExt').closest('.cell')!.setAttribute('data-help','Cell-temperature channel only when explicitly provided by the backend.');
for(const id of ['rCap','rWh'])$(id).closest('.cell')!.setAttribute('data-help','Browser-session integral of fresh samples. Gaps are excluded; not the native instrument counter.');
$('rTime').closest('.cell')!.setAttribute('data-help','Observed valid sample intervals in this browser session; gaps excluded.');
$('gear').dataset.help='All original settings remain visible. Unsupported backend actions are disabled and explained.';
$('stbyTime').closest('.srow')!.setAttribute('data-help','Raw value 0–120. Physical time unit is unconfirmed.');
$('langSel').closest('.srow')!.setAttribute('data-help','Backend language enum 0 or 1; no HID enum mapping is assumed.');
$('deviceSelect').title='Changing devices starts a new browser recording. Export CSV before switching.';
$('clearBtn').dataset.help='Clear chart, CSV buffer and browser-session integrals only. Device counters remain unchanged.';
$('zeroData').dataset.help='Reset device counters with confirmed OFF. Previous device counters cannot be restored; browser recording is unchanged.';
bind('connect',async()=>{await api.connect();if(api.connected)report('Connected. Export before changing device or closing this page.');});
bind('reserve',async()=>{if(api.lease){await api.release();report('Device stopped and released');}else{await api.reserve();report('Selected device reserved');}});
$<HTMLSelectElement>('deviceSelect').onchange=()=>{const id=input('deviceSelect').value;void run(async()=>{await api.select(id);report('Device selected; new browser recording started.');});};
bind('setBtn',()=>{const range=targetRange();return api.target(number('setVal',range.min,range.max,range.step));});
bind('loadBtn',()=>api.actor?.output==='on'?api.stop():api.enable(),true);
bind('stop',()=>api.stop(),true);
for(const mode of ['cc','cv','cr','cp'] as const)bind('mode-'+mode,async()=>{
 const result=await api.modeWrite(mode);
 if(mode==='cv'&&store('cvNotice')!=='off')$('cvToast').classList.add('show');
 return result;
});
bind('timeBtn',()=>{const hr=number('thr',0,24,1),min=number('tmin',0,59,1),seconds=(hr*60+min)*60;if(seconds>86400)throw Error('Timer must not exceed 24 hours');return api.duration(seconds);});
bind('limBtn',async()=>{
 const settings:Dl24mSettings={};for(const id of ['ocp','opp','otext','otmos','cutv'])if(input(id).value.trim())settings[settingsFields[id].key]=field(id);
 if(!Object.keys(settings).length)throw Error('Enter at least one protection setting');
 if(Object.keys(settings).length===1&&settings.cutoffVoltageV!==undefined)return api.cutoff(settings.cutoffVoltageV);
 return api.settingsWrite(settings);
});
bind('zeroData',async()=>{if(confirm('Reset device counters? Their previous contents cannot be restored. Browser recording is unchanged.'))return api.resetCounters();});
$('clearBtn').onclick=()=>{recordingGap=false;session.clear();chart.setPoints([]);val('rCap',0);val('rWh',0);$('rTime').textContent='00:00:00';report('Browser recording cleared; device counters unchanged');render();};
$('export').onclick=()=>{const url=URL.createObjectURL(new Blob([session.csv()],{type:'text/csv;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download=`dl24-${api.device?.id??'session'}.csv`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('pollRate').onchange=()=>{void run(()=>api.interval(Number(input('pollRate').value)));store('pollMs',input('pollRate').value);};
for(const b of Array.from(document.querySelectorAll<HTMLElement>('[data-pm]')))b.onclick=()=>{pollMode=b.dataset.pm!;session.break();document.querySelectorAll<HTMLElement>('[data-pm]').forEach(e=>e.classList.toggle('sel',e===b));render();};
for(const b of Array.from(document.querySelectorAll<HTMLElement>('[data-th]')))b.onclick=()=>setTheme(b.dataset.th!);
let returnFocus:HTMLElement|null=null;
function closeSettings(){$('settingsModal').hidden=true;returnFocus?.focus();}
$('gear').onclick=()=>{returnFocus=document.activeElement as HTMLElement;$('settingsModal').hidden=false;$('settingsClose').focus();};
$('settingsClose').onclick=closeSettings;$('settingsBackdrop').onclick=closeSettings;
addEventListener('keydown',e=>{
 if($('settingsModal').hidden)return;
 if(e.key==='Escape'){closeSettings();return;}
 if(e.key==='Tab'){
  const items=Array.from($('settingsModal').querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),select:not(:disabled),a[href]')).filter(e=>e.getClientRects().length);
  const first=items[0],last=items[items.length-1];if(e.shiftKey&&document.activeElement===first){e.preventDefault();last?.focus();}else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first?.focus();}
 }
});
$('cvDismiss').onclick=()=>$('cvToast').classList.remove('show');
$('cvNever').onclick=()=>{store('cvNotice','off');$('cvToast').classList.remove('show');};
for(const s of Array.from(document.querySelectorAll<HTMLElement>('.legend span'))){
 const b=document.createElement('button');b.textContent=s.textContent;b.style.cssText=s.style.cssText;b.dataset.s=s.dataset.s;b.setAttribute('aria-pressed','true');s.replaceWith(b);
 b.onclick=()=>{const off=b.classList.toggle('off');b.setAttribute('aria-pressed',String(!off));chart.setVisible(({1:'v',2:'i',3:'w'} as Record<string,string>)[b.dataset.s!],!off);};
}
for(const d of Array.from(document.querySelectorAll<HTMLDetailsElement>('details.grp'))){const name=d.querySelector('summary')!.textContent!;const saved=store('details:'+name);if(saved!==null)d.open=saved==='1';d.ontoggle=()=>{store('details:'+name,d.open?'1':'0');};}
document.body.addEventListener('mouseover',e=>{$('helpbar').textContent=(e.target as HTMLElement).closest<HTMLElement>('[data-help]')?.dataset.help??'Hover any control for a short description.';});
for(const a of Array.from(document.querySelectorAll<HTMLAnchorElement>('nav a')))a.onclick=e=>{if(api.lease){e.preventDefault();void run(async()=>{await api.release();location.href=a.href;});}};
addEventListener('pagehide',()=>api.unload());
new ResizeObserver(()=>chart.fit()).observe($('chart').parentElement!);
window.setInterval(render,250);
setTheme(store('theme')==='light'?'light':'dark');
const savedRate=Number(store('pollMs'));if([500,1000,2000].includes(savedRate)){api.pollMs=savedRate;input('pollRate').value=String(savedRate);}
render();chart.fit();

function renderAuth(){
 $('benchAuth').replaceChildren();
 api.auth.render($('benchAuth'),async()=>{
  renderAuth();
  // Authentication may resume discovery, never reservation or device commands.
  if(api.auth.state.authenticated){await api.connect();report('Connected. Reserve the selected device before making changes.');}
 });
 render();
}
api.auth.report=error=>{void message(error).then(text=>report(text,true));};
api.auth.notify=text=>report(text);
api.auth.onLost=()=>{
 ++request;localPending=false;api.endSession();renderAuth();
};
api.auth.beforeLogout=async()=>{
 try{await api.release();}finally{api.endSession();}
};
void (async()=>{
 await api.auth.refresh();renderAuth();
 if(api.auth.state.authenticated)await api.connect();
})().catch(api.auth.report);
