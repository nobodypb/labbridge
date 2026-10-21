// SPDX-License-Identifier: GPL-3.0-or-later
import {Dl24mApi} from '../../../generated/src/index';
import type {Dl24mSettings} from '../../../generated/src/index';
import type {BenchModuleContext,BenchModuleView} from './types';
import {features,settingsFields} from './dl24-features';

/** The retained upstream controls are owned exclusively by the DL24 module. */
export function mountDl24(context:BenchModuleContext):BenchModuleView {
 const {api,input,element,bind,number}=context;
 let dirty=new Set<string>();
 const field=(id:string)=>{const f=settingsFields[id];return number(id,f.min,f.max,f.step);};
 const disable=(id:string,disabled:boolean)=>{const e=element(id);if(e&&'disabled'in e)(e as HTMLButtonElement).disabled=disabled;};
 for(const el of Array.from(document.querySelectorAll<HTMLElement>('[data-mode]')))el.id='mode-'+el.dataset.mode;
 for(const [id,f] of Object.entries(features)){
  const el=element(id);if(!el)throw Error(`Missing Bench feature ${id}`);
  if(f.reason){el.title=f.reason;el.dataset.help=f.reason;disable(id,true);}
 }
 for(const id of ['calV','zeroNoLoad','restoreDefaults','snapBtn','mode-cv']){
  const note=document.createElement('p');note.className='hint';note.dataset.unavailable='true';note.dataset.featureNote=id;note.textContent=features[id].reason??'Connect to discover backend support.';
  (id.startsWith('mode-')?element(id).closest('.grp')!:element(id).closest('.sgrp')??element(id).parentElement!).append(note);
 }
 element('rawLive').textContent=element('rawSet').textContent='Passive BLE diagnostics: use Snapshot for bounded capture and cached registers.';
 element('snapState').textContent='No browser snapshot captured.';
 let previousRegisters=new Map<string,number|undefined>(),diagnosticExport:unknown,diagnosticIdentity='';
 bind('snapBtn',async()=>{
  const state=api.selected;if(!state||!api.connected||state.device.features?.find(f=>f.id==='diagnostics')?.status!=='supported')throw Error('Passive diagnostics unavailable');
  const boot=api.boot,d=await new Dl24mApi(api.config).getDl24mDiagnostics({deviceId:state.device.id});
  if(api.selected!==state||boot!==api.boot)throw Error('Device changed during diagnostic capture');
  const changes=d.registers.filter(r=>previousRegisters.has(r.name)&&previousRegisters.get(r.name)!==r.value);
  element('rawLive').textContent=d.entries.map(e=>`${e.sequence} ${e.receivedMs}ms gen ${e.generation} ${e.direction.toUpperCase()} ${e.hex}${e.truncated?' [truncated]':''}`).join('\n');
  element('rawSet').textContent=d.registers.map(r=>`${r.name} [${r.command.toString(16)}] ${r.value??'—'} · ${r.quality} · ${r.ageMs??'—'}ms`).join('\n');
  element('snapState').textContent=`${input('snapLabel').value||'Snapshot'} · ${d.entries.length} BLE chunks · ${d.dropped} overwritten · `+(previousRegisters.size?`${changes.length} changed: ${changes.map(r=>r.name).join(', ')||'none'}`:'first snapshot');
  element('snapDiff').textContent=changes.map(r=>`${r.name}: ${previousRegisters.get(r.name)??'—'} → ${r.value??'—'} (${r.quality})`).join('\n');
  previousRegisters=new Map(d.registers.map(r=>[r.name,r.value]));diagnosticExport={boot_id:boot,device_id:state.device.id,label:input('snapLabel').value,diagnostics:d};
 });
 element('snapClear').onclick=()=>{previousRegisters.clear();diagnosticExport=undefined;element('snapState').textContent='Browser snapshots cleared; instrument unchanged.';element('snapDiff').textContent='';};
 const exportButton=document.createElement('button');exportButton.textContent='Export BLE diagnostics';exportButton.type='button';element('snapClear').after(exportButton);
 exportButton.onclick=()=>{if(!diagnosticExport)return;const url=URL.createObjectURL(new Blob([JSON.stringify(diagnosticExport,null,2)],{type:'application/json'})),a=document.createElement('a');a.href=url;a.download='dl24m-ble-diagnostics.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};

 for(const [id,f] of Object.entries(settingsFields)){
  const e=input(id);e.addEventListener('input',()=>dirty.add(id));e.addEventListener('change',()=>dirty.add(id));e.min=String(f.min);e.max=String(f.max);e.step=String(f.step);
  if(f.button)bind(f.button,async()=>{const state=api.selected,result=await api.settingsWrite({[f.key]:field(id)});if(api.selected===state)dirty.delete(id);return result;});
 }
 for(const mode of ['cc','cv','cr','cp'] as const)bind('mode-'+mode,async()=>{
  if(mode!=='cc'&&!api.device?.features?.some(f=>f.id===mode&&f.status==='supported'))throw Error('Mode is not supported');
  const result=await api.command((config,params)=>new Dl24mApi(config).setDl24mMode({...params,dl24mModeRequest:{mode}}));
  let notice=true;try{notice=localStorage.getItem('bench:cvNotice')!=='off';}catch{}
  if(mode==='cv'&&notice)element('cvToast').classList.add('show');return result;
 });
 bind('limBtn',async()=>{
  const settings:Dl24mSettings={};for(const id of ['ocp','opp','otext','otmos','cutv'])if(input(id).value.trim())settings[settingsFields[id].key]=field(id);
  if(!Object.keys(settings).length)throw Error('Enter at least one protection setting');
  if(Object.keys(settings).length===1&&settings.cutoffVoltageV!==undefined)return api.cutoff(settings.cutoffVoltageV);
  return api.settingsWrite(settings);
 });
 bind('zeroData',async()=>{if(confirm('Reset device counters? Their previous contents cannot be restored. Browser recording is unchanged.'))return api.resetCounters();});
 element('stbyTime').closest('.srow')!.setAttribute('data-help','Raw value 0–120. Physical time unit is unconfirmed.');
 element('langSel').closest('.srow')!.setAttribute('data-help','Backend language enum 0 or 1; no HID enum mapping is assumed.');
 element('zeroData').dataset.help='Reset device counters with confirmed OFF. Previous counters cannot be restored; browser recording is unchanged.';
 return {
  render(pending){
   const off=api.off&&!pending&&api.module?.id==='dl24m',settings=api.settings as Dl24mSettings;
   disable('limBtn',!off);disable('zeroData',!off);disable('mode-cc',!off);
   for(const f of Object.values(settingsFields))if(f.button)disable(f.button,!off);
   const identity=JSON.stringify([api.device?.id,api.boot]);
   if(identity!==diagnosticIdentity){diagnosticIdentity=identity;previousRegisters.clear();diagnosticExport=undefined;element('snapDiff').textContent='';element('snapState').textContent='No browser snapshot captured for this device/boot.';element('rawLive').textContent=element('rawSet').textContent='Capture passive diagnostics for the selected device.';}
   for(const [id,f] of Object.entries(features)){
    const status=f.capability?api.device?.features?.find(s=>s.id===f.capability):undefined;
    const reason=f.capability?(status?.status==='supported'?'':status?.reason??f.reason):f.reason;
    if(f.capability||f.reason){disable(id,!!reason||!api.connected||id==='snapBtn'&&(pending||api.busy)||id.startsWith('mode-')&&!off);element(id).title=reason??status?.reason??'';element(id).dataset.help=element(id).title;}
   }
   document.querySelectorAll<HTMLElement>('[data-mode]').forEach(b=>{const mode=b.dataset.mode!;b.classList.toggle('sel',settings.mode===['cc','cv','cr','cp'].indexOf(mode));disable(b.id,!off||mode!=='cc'&&!api.device?.features?.some(f=>f.id===mode&&f.status==='supported'));});
   element('unsupported').textContent='Backend feature status: '+(api.device?.features?.filter(f=>f.status!=='supported').map(f=>`${f.id}: ${f.status} — ${f.reason}`).join(' · ')||'Backend support is discovered per instrument.');
   for(const note of Array.from(document.querySelectorAll<HTMLElement>('[data-feature-note]'))){const f=features[note.dataset.featureNote!];note.textContent=api.device?.features?.find(s=>s.id===f.capability)?.reason??f.reason??'Connect to discover backend support.';}
   const m=api.measurement;
   if(m?.loadTarget&&m.loadTarget.mode===['cc','cv','cr','cp'][settings.mode??-1])input('setVal').placeholder=String(m.loadTarget.value);
   const native=api.connected?m?.nativeReadings??[]:[];
   element('nativeReadings').textContent=native.length?'Native device readings · '+native.map(r=>{const fresh=r.quality==='valid'&&r.ageMs!==undefined&&r.ageMs+performance.now()-api.selected!.receivedAt<=(api.device?.limits?.maxAgeMs??0);return `${r.id}: ${fresh?r.value:'—'} ${r.unit} [${r.origin}; ${fresh?'valid':r.quality==='unavailable'?'unavailable':'stale'}]`;}).join(' · '):'Native device readings: —';
   for(const [id,f] of Object.entries(settingsFields)){
    const e=input(id),value=settings[f.key];
    if(e.tagName==='SELECT'){if(!dirty.has(id))e.value=value===undefined?'':String(value);}else e.placeholder=value===undefined?'—':String(value);
   }
   element('rCut').textContent=Number.isFinite(settings.cutoffVoltageV)?settings.cutoffVoltageV!.toFixed(1):'—';
  },
  saveDraft(draft){for(const id of Object.keys(settingsFields))draft['dl24:'+id]=input(id).value;draft['dl24:dirty']=JSON.stringify([...dirty]);},
  restoreDraft(draft){for(const id of Object.keys(settingsFields))input(id).value=draft['dl24:'+id]??'';dirty=new Set(JSON.parse(draft['dl24:dirty']??'[]'));}
 };
}
