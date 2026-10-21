// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import type {Measurements} from '../../generated/src/index';
import {BenchAdapter,message} from './adapter';
import {benchModules} from './modules';
import type {BenchModuleView} from './modules/types';
import {MeasurementView} from './measurements';
import {BenchLayout} from './layout';
const $=<T extends HTMLElement=HTMLElement>(id:string)=>document.getElementById(id) as T;
const input=(id:string)=>$<HTMLInputElement>(id);
const button=(id:string)=>$<HTMLButtonElement>(id);
const api=new BenchAdapter();
const layout=new BenchLayout();
const feedback=new Map<string,{text:string;error:boolean}>();
let reportedError=false;
let moduleViews:BenchModuleView[]=[];
let pollMode='live',request=0,recordingGap=false;
const pendingDevices=new Map<string,number>();
const view=new MeasurementView(api),chart=view.chart;
const draftIds=['setVal','voltageVal','thr','tmin'];
function saveDraft(){const state=api.selected;if(!state)return;for(const id of draftIds)state.draft[state.componentId+':'+id]=input(id).value;for(const v of moduleViews)v.saveDraft(state.draft);}
function restoreDraft(){const state=api.selected;for(const id of draftIds)input(id).value=state?.draft[(state.componentId??'')+':'+id]??'';for(const v of moduleViews)v.restoreDraft(state?.draft??{});}
function report(text:string,error=false,deviceId?:string){
 $('benchStatus').textContent=text;$('benchStatus').classList.toggle('error',error);$('globalNotice').textContent=text;reportedError=error;
 if(deviceId)feedback.set(deviceId,{text,error});
 $('statusMenu').classList.toggle('has-error',error||[...api.states.values()].some(s=>s.error||s.controlError));
}
async function run(fn:()=>Promise<unknown>,priority=false){
 const deviceId=api.device?.id??'',previous=pendingDevices.get(deviceId);
 if((previous!==undefined||api.auth.loggingOut)&&!priority)return;
 const n=++request,working=`${deviceId||'Node'}: working…`;pendingDevices.set(deviceId,n);feedback.delete(deviceId);report(working);render();
 try{const result=await fn();if(pendingDevices.get(deviceId)===n&&result&&typeof result==='object'&&'status'in result)report(`${deviceId}: operation ${result.status}`,false,deviceId);else if(result===undefined&&pendingDevices.get(deviceId)===n&&$('benchStatus').textContent===working)report('No device command submitted');}
 catch(e){if(pendingDevices.get(deviceId)===n)report(`${deviceId}: ${await message(e)}`,true,deviceId);}
 finally{if(pendingDevices.get(deviceId)===n)pendingDevices.delete(deviceId);render();}
}
function bind(id:string,fn:()=>Promise<unknown>,priority=false){button(id).onclick=()=>void run(fn,priority);}
function store(key:string,value?:string){try{if(value!==undefined)localStorage.setItem('bench:'+key,value);return localStorage.getItem('bench:'+key);}catch{return null;}}
function setTheme(theme:string){document.documentElement.dataset.theme=theme;store('theme',theme);document.querySelectorAll<HTMLElement>('[data-th]').forEach(b=>b.classList.toggle('sel',b.dataset.th===theme));chart.redraw();}
function number(id:string,min:number,max:number,step:number){
 const el=input(id),n=Number(el.value);
 if(el.value.trim()===''||!Number.isFinite(n)||n<min||n>max||step>0&&Math.abs((n-min)/step-Math.round((n-min)/step))>1e-6)throw Error(`Enter a valid value for ${el.getAttribute('aria-label')||id}: ${min}–${max}, step ${step}`);
 return n;
}
function disable(id:string,disabled:boolean){const e=$(id);if(e&&'disabled'in e)(e as HTMLButtonElement).disabled=disabled;}
function val(id:string,value:number|undefined,decimals=3){$(id).textContent=Number.isFinite(value)?value!.toFixed(decimals):'—';}
function temperature(m:Measurements,kind:string){const t=m.temperatureChannels.find(c=>c.kind===kind&&c.quality==='valid'&&c.ageMs<=(api.device?.limits?.maxAgeMs??0));return t?.valueC;}
function elapsed(ms:number){const sec=Math.floor(ms/1000);return [Math.floor(sec/3600),Math.floor(sec/60)%60,sec%60].map(v=>String(v).padStart(2,'0')).join(':');}
function render(){
 const pending=pendingDevices.has(api.device?.id??'')||api.auth.loggingOut,ready=api.writable&&!pending,off=api.off&&!pending;
 $('connLbl').textContent=api.connected&&!api.connecting?'Connected':'Connect';$('connect').classList.toggle('on',api.connected);$('connect').classList.toggle('off',!api.connected);
 disable('connect',pending||api.busy||api.connecting||!api.auth.state.authenticated);
 const select=$<HTMLSelectElement>('deviceSelect');
 if(Array.from(select.options).map(o=>o.value).join(',')!==api.devices.map(d=>d.id).join(',')){
  select.replaceChildren(...api.devices.map(d=>{const o=document.createElement('option');o.value=d.id;o.textContent=`${d.id} · ${d.identity}`;return o;}));
 }
 select.value=api.device?.id??'';select.disabled=api.auth.loggingOut||api.connecting||!api.connected;
 const component=$<HTMLSelectElement>('componentSelect');
 if(Array.from(component.options).map(o=>o.value).join(',')!==api.actors.map(c=>c.id).join(','))component.replaceChildren(...api.actors.map(c=>{const o=document.createElement('option');o.value=c.id;o.textContent=c.id;return o;}));
 component.value=api.componentId??'';$('componentChoice').hidden=api.actors.length<2;component.disabled=api.auth.loggingOut;
 document.querySelectorAll<HTMLElement>('[data-module]').forEach(el=>el.hidden=el.dataset.module!==api.module?.id);
 $('noModuleSettings').hidden=!!api.module?.writeSettings;
 $('voltageControl').hidden=!api.supports('voltage');$('durationControl').hidden=!api.supports('duration');
 const range=api.device?api.module?.target?.(api.device,api.settings):undefined,step=range?.step??api.module?.currentStep??0;input('setVal').step=step?String(step):'any';input('setVal').min=String(range?.min??0);$('setUnit').textContent=`(${range?.unit??'A'})`;
 input('voltageVal').step=api.module?.voltageStep?String(api.module.voltageStep):'any';input('voltageVal').max=String(api.device?.limits?.voltageV??0);
 disable('voltageBtn',!ready||!api.supports('voltage'));
 $('moduleStatus').textContent=api.fresh&&api.measurement?(api.module?.status?.(api.measurement)??[]).map(s=>`${s.label}: ${s.value}`).join(' · '):'';
 disable('stopAll',!api.auth.state.authenticated||!api.reserved.length);
 const state=api.selected,notice=state?.controlError||feedback.get(api.device?.id??'')?.text||'';
 $('deviceState').textContent=api.device?`${api.actor?.output?.toUpperCase()??'UNKNOWN'} · ${api.lease?'reserved':state?.owned?'reservation uncertain':'not reserved'}`:'No device selected';
 $('deviceFeedback').textContent=notice;$('deviceFeedback').classList.toggle('error',!!state?.controlError||!!feedback.get(api.device?.id??'')?.error);
 const attention=reportedError||[...api.states.values()].some(s=>s.error||s.controlError||s.uncertain);
 $('statusMenu').classList.toggle('has-error',attention);$('statusMenu').title=attention?'Attention required · open node status':'Node and instrument status';
 $('moreMenu').classList.toggle('has-error',attention||view.recording.full);
 $('recordingMenu').classList.toggle('has-error',view.recording.full);$('recordingMenu').title=view.recording.full?'Recording paused · buffer full':'Recording options';
 $('reserve').textContent=api.lease?'Release and stop':'Reserve device';disable('reserve',pending||api.busy||api.shuttingDown||!api.connected||!api.componentId||!api.fresh||api.uncertain&&!api.lease);
 disable('stop',!api.componentId||!api.auth.state.authenticated);disable('setBtn',!ready||!api.supports('current')||!api.module?.canControl(api.settings,api.device)||range?.live===false&&!off);
 input('setVal').max=String(range?.max??api.device?.limits?.currentA??0);
 disable('loadBtn',api.actor?.output==='on'?!api.auth.state.authenticated:!api.canOn||pending||!api.supports('output'));
 $('loadBtn').classList.toggle('running',api.actor?.output==='on');$('loadBtn').classList.toggle('stopped',api.actor?.output==='off');
 const outputLabel=api.actors.find(c=>c.id===api.componentId)?.kind==='sink'?'LOAD':'OUTPUT';
 $('loadBtn').querySelector('.lbl')!.textContent=api.busy?`${outputLabel} · confirming…`:`${outputLabel} ${api.actor?.output?.toUpperCase()??'UNKNOWN'}`;
 disable('timeBtn',!off||!api.supports('duration'));

 disable('export',view.recording.rows.length===0);
 if(api.actor?.settings.timeLimitS!==undefined){input('thr').placeholder=String(Math.floor(api.actor.settings.timeLimitS/3600));input('tmin').placeholder=String(Math.floor(api.actor.settings.timeLimitS%3600/60));}
 $('setConn').textContent=api.connected?`${api.device?.id} · ${api.actor?.output??'unknown'}${api.lease?' · reserved':''}`:'device not connected';
 input('setVal').placeholder=api.actor?.settings.currentA===undefined?'—':String(api.actor.settings.currentA);
 input('voltageVal').placeholder=api.actor?.settings.voltageV===undefined?'—':String(api.actor.settings.voltageV);
 $('dataQuality').textContent=api.connecting?'Discovering devices…':!api.connected?'Disconnected — readings unavailable':api.componentId&&!api.fresh?'Stale — control locked':pollMode==='load'&&api.componentId&&api.actor?.output!=='on'?'Selected device recording paused · monitoring and other devices active':api.componentId?'Live · fresh measurements':'Monitoring sensor channels · freshness shown per channel';
 if(!api.fresh){for(const id of ['rV','rI','rW','rR','rT','rExt'])$(id).textContent='—';}
 $('recordingStatus').textContent=view.recording.full?'Recording paused: 100,000 channel values reached. Export CSV, then Clear to record again.':recordingGap?'Stream gap · browser integrals exclude missing intervals.':'';
 const m=api.measurement,good=api.fresh;
 val('rV',good?m?.voltageV:undefined);val('rI',good?m?.currentA:undefined,4);val('rW',good?m?.powerW:undefined);
 val('rR',good&&m?.currentA&&m.voltageV!==undefined?m.voltageV/m.currentA:undefined);
 const fine=m?.nativeReadings?.find(r=>r.id==='temperature_magnitude'&&r.quality==='valid'&&r.ageMs!==undefined&&r.ageMs<=(api.device?.limits?.maxAgeMs??0)&&Number.isFinite(r.value));val('rT',good?(fine?.value??(m?temperature(m,'internal'):undefined)):undefined,fine?2:1);val('rExt',good&&m?(temperature(m,'external_probe')??temperature(m,'cell')):undefined,1);
 const integral=view.recording.integrals.get(api.device?.id??'');val('rCap',integral?integral.chargeAh*1000:undefined);val('rWh',integral?.energyWh);$('rTime').textContent=integral?elapsed(integral.elapsedMs):'—';
 for(const v of moduleViews)v.render(pending);view.render();
}
api.onGap=deviceId=>{recordingGap=true;view.recording.break(deviceId);render();};
api.onChange=render;
api.onError=(text,deviceId)=>{view.recording.break(deviceId);report(text,true,deviceId);};
api.onSample=state=>{view.sample(state,pollMode);render();};
moduleViews=benchModules.flatMap(module=>module.mount?[module.mount({api,input,element:$,bind,number})]:[]);
// Associate original visual labels with their controls for keyboard/screen-reader use.
for(const row of Array.from(document.querySelectorAll<HTMLElement>('.srow,.setrow>div'))){
 const label=row.querySelector('label'),control=row.querySelector<HTMLInputElement>('input,select');
 if(label&&control){label.htmlFor=control.id;control.setAttribute('aria-label',label.textContent?.trim()??control.id);}
}
const quality=document.createElement('div');quality.id='dataQuality';quality.className='hint';$('deviceQuality').append(quality);
const recording=document.createElement('div');recording.id='recordingStatus';recording.className='hint';$('recordingOptions').append(recording);
input('thr').max='24';
$('connect').dataset.help='Refresh device discovery using your signed-in LabBridge session. Reserve explicitly before changing a device.';
$('pollRate').closest('label')!.dataset.help='Refresh all device caches and selected-device pushes at this interval over WebSocket; this does not change instrument polling.';
$('pollRate').closest('label')!.title='WebSocket measurement interval';$('pollMode').dataset.help='Live records all channels continuously. While output ON records each actor device during its own confirmed ON; standalone sensors, monitoring and heartbeats continue.';
$('pollMode').title=$('pollMode').dataset.help!;
$('rT').closest('.cell')!.setAttribute('data-help','Internal instrument temperature; not a cell-temperature channel.');
$('rExt').closest('.cell')!.setAttribute('data-help','External probe or explicit cell-temperature channel. An external probe is not automatically a cell guard.');
for(const id of ['rCap','rWh'])$(id).closest('.cell')!.setAttribute('data-help','Browser-session integral of fresh samples. Gaps are excluded; not the native instrument counter.');
$('rTime').closest('.cell')!.setAttribute('data-help','Observed valid sample intervals in this browser session; gaps excluded.');
$('gear').dataset.help='All original settings remain visible. Unsupported backend actions are disabled and explained.';
$('deviceSelect').title='Select controls only. Other outputs, reservations and recordings keep running.';
$('clearBtn').dataset.help='Clear chart, CSV buffer and browser-session integrals only. Device counters remain unchanged.';
bind('connect',async()=>{await api.auth.refresh();renderAuth();if(api.auth.state.authenticated){await api.connect();restoreDraft();if(api.connected)report('Connected. Select and reserve devices independently. Export before closing this page.');}});
bind('reserve',async()=>{if(api.lease){await api.release();report('Device stopped and released');}else{await api.reserve();report('Selected device reserved');}});
$<HTMLSelectElement>('deviceSelect').onchange=()=>{const id=input('deviceSelect').value;saveDraft();void api.select(id).then(()=>{restoreDraft();render();report('Controls selected; other devices and recording continue.');}).catch(e=>void message(e).then(t=>report(t,true)));};
 $<HTMLSelectElement>('componentSelect').onchange=()=>{saveDraft();api.selectComponent(input('componentSelect').value);restoreDraft();render();};
bind('setBtn',()=>api.target(number('setVal',Number(input('setVal').min),Number(input('setVal').max),Number(input('setVal').step)||0)));
bind('loadBtn',()=>api.actor?.output==='on'?api.stop():api.enable(),true);
bind('stop',()=>api.stop(),true);
 bind('stopAll',async()=>{await api.stopAll();report('All reserved devices confirmed OFF');},true);
 bind('voltageBtn',()=>api.voltage(number('voltageVal',0,api.device?.limits?.voltageV??0,api.module?.voltageStep??0)));
bind('timeBtn',()=>{const hr=number('thr',0,24,1),min=number('tmin',0,59,1),seconds=(hr*60+min)*60;if(seconds>86400)throw Error('Timer must not exceed 24 hours');return api.duration(seconds);});
$('clearBtn').onclick=()=>{recordingGap=false;view.clear();report('Browser recording cleared; device counters unchanged');render();};
$('export').onclick=()=>{const url=URL.createObjectURL(new Blob([view.recording.csv()],{type:'text/csv;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download='labbridge-bench.csv';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('pollRate').onchange=()=>{void run(()=>api.interval(Number(input('pollRate').value)));store('pollMs',String(api.pollMs));};
for(const b of Array.from(document.querySelectorAll<HTMLElement>('[data-pm]')))b.onclick=()=>{pollMode=b.dataset.pm!;view.recording.break();document.querySelectorAll<HTMLElement>('[data-pm]').forEach(e=>e.classList.toggle('sel',e===b));render();};
for(const b of Array.from(document.querySelectorAll<HTMLElement>('[data-th]')))b.onclick=()=>setTheme(b.dataset.th!);
let returnFocus:HTMLElement|null=null;
function closeSettings(){$('settingsModal').hidden=true;returnFocus?.focus();}
$('gear').onclick=()=>{layout.closeMenus();returnFocus=document.activeElement as HTMLElement;$('settingsModal').hidden=false;$('settingsClose').focus();};
$('settingsClose').onclick=closeSettings;$('settingsBackdrop').onclick=closeSettings;
addEventListener('keydown',e=>{
 if($('settingsModal').hidden)return;
 if(e.key==='Escape'){closeSettings();return;}
 if(e.key==='Tab'){
  const items=Array.from($('settingsModal').querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),select:not(:disabled),a[href]')).filter(e=>e.getClientRects().length);
  const first=items[0],last=items[items.length-1];if(e.shiftKey&&document.activeElement===first){e.preventDefault();last?.focus();}else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first?.focus();}
 }
});
$('cvDismiss').onclick=()=>$('cvToast').classList.remove('show');$('cvNever').onclick=()=>{store('cvNotice','off');$('cvToast').classList.remove('show');};
for(const d of Array.from(document.querySelectorAll<HTMLDetailsElement>('details.grp'))){const name=d.querySelector('summary')!.textContent!;const saved=store('details:'+name);if(saved!==null)d.open=saved==='1';d.ontoggle=()=>{store('details:'+name,d.open?'1':'0');};}
document.body.addEventListener('mouseover',e=>{$('helpbar').textContent=(e.target as HTMLElement).closest<HTMLElement>('[data-help]')?.dataset.help??'Hover any control for a short description.';});
for(const a of Array.from(document.querySelectorAll<HTMLAnchorElement>('nav a')))a.onclick=e=>{if(api.reserved.length){e.preventDefault();void run(async()=>{await api.releaseAll();location.href=a.href;},true);}};
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
  if(api.auth.state.authenticated){await api.connect();restoreDraft();report('Connected. Reserve the selected device before making changes.');}
 });
 layout.account(api.auth.state.authenticated);
 render();
}
api.auth.report=error=>{void message(error).then(text=>{$('authFeedback').textContent=text;$('authFeedback').classList.add('error');report(text,true);});};
api.auth.notify=text=>{$('authFeedback').textContent=text;$('authFeedback').classList.remove('error');report(text);};
api.auth.onLost=()=>{
 ++request;pendingDevices.clear();view.recording.break();api.endSession();renderAuth();
};
api.auth.beforeLogout=async()=>{
 try{await api.releaseAll();}finally{api.endSession();}
};
void (async()=>{
 await api.auth.refresh();renderAuth();
 if(api.auth.state.authenticated){await api.connect();restoreDraft();render();}
})().catch(api.auth.report);
