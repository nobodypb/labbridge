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
let session=new BenchSession(3000),sessionIdentity='',pollMode='live',localPending=false,request=0;
const chart=new CanvasChart($<HTMLCanvasElement>('chart'),{maxpts:900,css:(v:string)=>getComputedStyle(document.documentElement).getPropertyValue(v).trim(),series:[{key:'v',cssVar:'--v'},{key:'i',cssVar:'--i'},{key:'w',cssVar:'--w'}]});
function report(text:string,error=false){$('benchStatus').textContent=text;$('benchStatus').classList.toggle('error',error);}
async function run(fn:()=>Promise<unknown>,priority=false){
 if(localPending&&!priority)return;
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
function field(id:string){const f=settingsFields[id];return number(id,f.min,f.max,f.step);}
function disable(id:string,disabled:boolean){const e=$(id);if(e&&'disabled'in e)(e as HTMLButtonElement).disabled=disabled;}
function val(id:string,value:number|undefined,decimals=3){$(id).textContent=Number.isFinite(value)?value!.toFixed(decimals):'—';}
function temperature(m:Measurements,kind:string){const t=m.temperatureChannels.find(c=>c.kind===kind&&c.quality==='valid'&&c.ageMs<=(api.device?.limits?.maxAgeMs??0));return t?.valueC;}
function elapsed(ms:number){const sec=Math.floor(ms/1000);return [Math.floor(sec/3600),Math.floor(sec/60)%60,sec%60].map(v=>String(v).padStart(2,'0')).join(':');}
function render(){
 const ready=api.writable&&!localPending,off=api.off&&!localPending;
 $('connLbl').textContent=api.connected?'Connected':'Connect';$('connect').classList.toggle('on',api.connected);$('connect').classList.toggle('off',!api.connected);
 disable('connect',localPending||api.busy);disable('apiToken',!!api.lease||localPending);
 const select=$<HTMLSelectElement>('deviceSelect');
 if(Array.from(select.options).map(o=>o.value).join(',')!==api.devices.map(d=>d.id).join(',')){
  select.replaceChildren(...api.devices.map(d=>{const o=document.createElement('option');o.value=d.id;o.textContent=`${d.id} · ${d.identity}`;return o;}));
 }
 select.value=api.device?.id??'';select.disabled=localPending||api.busy||!api.connected;
 $('reserve').textContent=api.lease?'Release and stop':'Reserve device';disable('reserve',localPending||api.busy||!api.connected||!api.fresh||api.uncertain&&!api.lease);
 disable('stop',!api.device||!api.token);disable('setBtn',!ready||!api.supports('current')||api.settings.mode!==0);
 input('setVal').max=String(api.device?.limits?.currentA??0);
 disable('loadBtn',api.actor?.output==='on'?false:!api.canOn||localPending||!api.supports('output'));
 $('loadBtn').classList.toggle('running',api.actor?.output==='on');$('loadBtn').classList.toggle('stopped',api.actor?.output==='off');
 $('loadBtn').querySelector('.lbl')!.textContent=api.busy?'LOAD · confirming…':`LOAD ${api.actor?.output?.toUpperCase()??'UNKNOWN'}`;
 disable('timeBtn',!off||!api.supports('duration'));disable('limBtn',!off);disable('zeroData',!off);
 for(const f of Object.values(settingsFields))if(f.button)disable(f.button,!off);
 disable('mode-cc',!off);document.querySelectorAll<HTMLElement>('[data-mode]').forEach(b=>b.classList.toggle('sel',api.settings.mode===0&&b.dataset.mode==='cc'));
 for(const [id,f] of Object.entries(features))if(f.reason)disable(id,true);
 disable('export',session.rows.length===0);
 $('setConn').textContent=api.connected?`${api.device?.id} · ${api.actor?.output??'unknown'}${api.lease?' · reserved':''}`:'device not connected';
 for(const [id,f] of Object.entries(settingsFields)){
  const e=input(id),v=api.settings[f.key];
  if(e.tagName==='SELECT'){if(!dirtySettings.has(id))e.value=v===undefined?'':String(v);}
  else e.placeholder=v===undefined?'—':String(v);
 }
 if(api.settings.timeLimitS!==undefined){input('thr').placeholder=String(Math.floor(api.settings.timeLimitS/3600));input('tmin').placeholder=String(Math.floor(api.settings.timeLimitS%3600/60));}
 if(api.actor?.settings.currentA!==undefined)input('setVal').placeholder=String(api.actor.settings.currentA);
 val('rCut',api.settings.cutoffVoltageV,1);
 $('dataQuality').textContent=!api.connected?'Disconnected — readings unavailable':!api.fresh?'Stale — control locked':pollMode==='load'&&api.actor?.output!=='on'?'Load recording paused · state monitoring active':'Live · fresh measurements';
 if(!api.fresh){for(const id of ['rV','rI','rW','rR','rT','rExt'])$(id).textContent='—';}
 $('recordingStatus').textContent=session.full?'Recording paused: 100,000 samples reached. Export CSV, then Clear to record again.':'';
}
api.onChange=render;
api.onError=text=>{session.break();chart.setPoints(session.points);report(text,true);};
api.onSample=m=>{
 const identity=JSON.stringify([m.deviceId,m.bootId]);
 if(identity!==sessionIdentity){dirtySettings.clear();for(const id of Object.keys(settingsFields))if(input(id).tagName!=='SELECT')input(id).value='';input('setVal').value='';input('thr').value='';input('tmin').value='';session=new BenchSession(api.device?.limits?.maxAgeMs??3000);sessionIdentity=identity;chart.setPoints([]);}
 if(pollMode==='load'&&api.actor?.output!=='on'){if(session.previous)session.break();return;}
 const good=api.fresh;
 val('rV',good?m.voltageV:undefined);val('rI',good?m.currentA:undefined,4);val('rW',good?m.powerW:undefined);
 val('rR',good&&m.currentA!==undefined&&m.currentA!==0&&m.voltageV!==undefined?m.voltageV/m.currentA:undefined);
 val('rT',temperature(m,'internal'),1);val('rExt',temperature(m,'cell'),1);
 session.add({...m,internalTemperatureC:temperature(m,'internal'),externalTemperatureC:temperature(m,'cell')});
 chart.setPoints(session.points);val('rCap',session.chargeAh*1000);val('rWh',session.energyWh);$('rTime').textContent=elapsed(session.elapsedMs);
};
for(const el of Array.from(document.querySelectorAll<HTMLElement>('[data-mode]')))el.id='mode-'+el.dataset.mode;
for(const [id,f] of Object.entries(features)){
 const el=$(id);if(!el)throw Error(`Missing Bench feature ${id}`);
 if(f.reason){el.title=f.reason;el.dataset.help=f.reason;disable(id,true);}
}
for(const id of ['calV','zeroNoLoad','restoreDefaults','snapBtn','mode-cv']){
 const note=document.createElement('p');note.className='hint';note.dataset.unavailable='true';note.textContent=features[id].reason!;
 const parent=id.startsWith('mode-')?$(id).closest('.grp')!:$(id).closest('.sgrp')??$(id).parentElement!;
 parent.append(note);
}
$('rawLive').textContent=$('rawSet').textContent=$('snapState').textContent='Unavailable: backend does not expose HID frames.';
for(const [id,f] of Object.entries(settingsFields)){
 const e=input(id);e.addEventListener('input',()=>dirtySettings.add(id));e.addEventListener('change',()=>dirtySettings.add(id));e.min=String(f.min);e.max=String(f.max);e.step=String(f.step);
 if(f.button)bind(f.button,async()=>{const result=await api.settingsWrite({[f.key]:field(id)});dirtySettings.delete(id);return result;});
}
// Associate original visual labels with their controls for keyboard/screen-reader use.
for(const row of Array.from(document.querySelectorAll<HTMLElement>('.srow,.setrow>div'))){
 const label=row.querySelector('label'),control=row.querySelector<HTMLInputElement>('input,select');
 if(label&&control){label.htmlFor=control.id;control.setAttribute('aria-label',label.textContent?.trim()??control.id);}
}
const quality=document.createElement('div');quality.id='dataQuality';quality.className='hint';$('benchStatus').after(quality);
const recording=document.createElement('div');recording.id='recordingStatus';recording.className='hint';quality.after(recording);
input('thr').max='24';
$('connect').dataset.help='Connect to this LabBridge node using the API token. No USB browser access is required.';
$('pollRate').closest('label')!.dataset.help='Read the backend cache at this interval; this does not change device polling.';
$('pollRate').closest('label')!.title='HTTP refresh interval';$('pollMode').dataset.help='Live records fresh samples continuously. Load records only while confirmed ON; state monitoring and lease heartbeat continue.';
$('pollMode').title=$('pollMode').dataset.help!;
$('rT').closest('.cell')!.setAttribute('data-help','Internal instrument temperature; not a cell-temperature channel.');
$('rExt').closest('.cell')!.setAttribute('data-help','Cell-temperature channel only when explicitly provided by the backend.');
for(const id of ['rCap','rWh'])$(id).closest('.cell')!.setAttribute('data-help','Browser-session integral of fresh samples. Gaps are excluded; not the native instrument counter.');
$('rTime').closest('.cell')!.setAttribute('data-help','Observed valid sample intervals in this browser session; gaps excluded.');
$('gear').dataset.help='All original settings remain visible. Unsupported backend actions are disabled and explained.';
$('stbyTime').closest('.srow')!.setAttribute('data-help','Raw value 0–120. Physical time unit is unconfirmed.');
$('langSel').closest('.srow')!.setAttribute('data-help','Backend language enum 0 or 1; no HID enum mapping is assumed.');
$('deviceSelect').title='Changing devices starts a new browser recording. Export CSV before switching.';
$('clearBtn').dataset.help='Clear chart, CSV buffer and browser-session integrals only. Device counters remain unchanged.';
$('zeroData').dataset.help='Reset device counters with confirmed OFF. Previous device counters cannot be restored; browser recording is unchanged.';
bind('connect',async()=>{await api.connect(input('apiToken').value);if(api.connected)report('Connected. Export before changing device or closing this page.');});
bind('reserve',async()=>{if(api.lease){await api.release();report('Device stopped and released');}else{await api.reserve();report('Selected device reserved');}});
$<HTMLSelectElement>('deviceSelect').onchange=()=>{const id=input('deviceSelect').value;void run(async()=>{await api.select(id);report('Device selected; new browser recording started.');});};
bind('setBtn',()=>api.current(number('setVal',0,api.device?.limits?.currentA??0,.001)));
bind('loadBtn',()=>api.actor?.output==='on'?api.stop():api.enable(),true);
bind('stop',()=>api.stop(),true);
bind('mode-cc',()=>api.settingsWrite({mode:0}));
bind('timeBtn',()=>{const hr=number('thr',0,24,1),min=number('tmin',0,59,1),seconds=(hr*60+min)*60;if(seconds>86400)throw Error('Timer must not exceed 24 hours');return api.duration(seconds);});
bind('limBtn',async()=>{
 const settings:Dl24mSettings={};for(const id of ['ocp','opp','otext','otmos','cutv'])if(input(id).value.trim())settings[settingsFields[id].key]=field(id);
 if(!Object.keys(settings).length)throw Error('Enter at least one protection setting');
 if(Object.keys(settings).length===1&&settings.cutoffVoltageV!==undefined)return api.cutoff(settings.cutoffVoltageV);
 return api.settingsWrite(settings);
});
bind('zeroData',async()=>{if(confirm('Reset device counters? Their previous contents cannot be restored. Browser recording is unchanged.'))return api.resetCounters();});
$('clearBtn').onclick=()=>{session.clear();chart.setPoints([]);val('rCap',0);val('rWh',0);$('rTime').textContent='00:00:00';report('Browser recording cleared; device counters unchanged');render();};
$('export').onclick=()=>{const url=URL.createObjectURL(new Blob([session.csv()],{type:'text/csv;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download=`dl24-${api.device?.id??'session'}.csv`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('pollRate').onchange=()=>{api.pollMs=Number(input('pollRate').value);store('pollMs',String(api.pollMs));};
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
$('cvDismiss').onclick=$('cvNever').onclick=()=>$('cvToast').classList.remove('show');
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
