// SPDX-License-Identifier: GPL-3.0-or-later
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
 for(const id of ['calV','zeroNoLoad','restoreDefaults','snapBtn']){
  const note=document.createElement('p');note.className='hint';note.dataset.unavailable='true';note.textContent=features[id].reason!;
  (id.startsWith('mode-')?element(id).closest('.grp')!:element(id).closest('.sgrp')??element(id).parentElement!).append(note);
 }
 element('rawLive').textContent=element('rawSet').textContent=element('snapState').textContent='Unavailable: backend does not expose HID frames.';
 for(const [id,f] of Object.entries(settingsFields)){
  const e=input(id);e.addEventListener('input',()=>dirty.add(id));e.addEventListener('change',()=>dirty.add(id));e.min=String(f.min);e.max=String(f.max);e.step=String(f.step);
  if(f.button)bind(f.button,async()=>{const state=api.selected,result=await api.settingsWrite({[f.key]:field(id)});if(api.selected===state)dirty.delete(id);return result;});
 }
 bind('mode-cc',()=>api.settingsWrite({mode:0}));
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
   for(const [id,f] of Object.entries(features))if(f.reason)disable(id,true);
   document.querySelectorAll<HTMLElement>('[data-mode]').forEach(b=>b.classList.toggle('sel',settings.mode===0&&b.dataset.mode==='cc'));
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
