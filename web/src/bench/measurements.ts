// SPDX-License-Identifier: GPL-3.0-or-later
import {BenchRecording} from './recording.js';
import {CanvasChart} from './chart.js';
import {moduleFor} from './modules';
import type {BenchAdapter,BenchDevice} from './adapter';
interface ChannelView {tile:HTMLButtonElement;value:HTMLElement;quality:HTMLElement;tileChoice:HTMLInputElement;curveChoice:HTMLInputElement;}
export class MeasurementView {
 readonly recording=new BenchRecording();
 readonly chart:CanvasChart;
 private channels=new Map<string,ChannelView>();
 private groups=new Map<string,{row:HTMLElement;heading:HTMLElement;quality:HTMLElement;tiles:HTMLElement;choices:HTMLElement}>();
 private overview=new Map<string,HTMLElement>();
 constructor(private api:BenchAdapter){
  this.chart=new CanvasChart(document.getElementById('chart'),{css:(v:string)=>getComputedStyle(document.documentElement).getPropertyValue(v).trim()});
 }
 private saved(key:string,fallback:boolean){try{const s=localStorage.getItem('bench:channel:'+key);return s===null?fallback:s==='1';}catch{return fallback;}}
 private hue(key:string){let hash=2166136261;for(let n=0;n<key.length;n++)hash=Math.imul(hash^key.charCodeAt(n),16777619);return String((hash>>>0)%36000/100);}
 private ensure(state:BenchDevice){
  let group=this.groups.get(state.device.id);
  if(!group){
   const section=document.createElement('section'),h=document.createElement('h3'),name=document.createElement('span'),quality=document.createElement('small'),tiles=document.createElement('div');
   section.className='measurement-strip';section.setAttribute('aria-label',state.device.id+' measurements');h.className='measurement-id';name.textContent=state.device.id;h.title=state.device.id;h.append(name,quality);tiles.className='channel-tiles';section.append(h,tiles);document.getElementById('measurements')!.append(section);
   const choices=document.createElement('fieldset'),legend=document.createElement('legend');legend.textContent=state.device.id;choices.append(legend);document.getElementById('channelChoices')!.append(choices);
   group={row:section,heading:h,quality,tiles,choices};this.groups.set(state.device.id,group);
  }
  for(const [key,c] of this.recording.channels){
   if(c.latest.deviceId!==state.device.id||this.channels.has(key))continue;
   const r=c.latest,defaultCurve=['voltage','current'].includes(r.componentId),hue=this.hue(key);
   const tile=document.createElement('button'),label=document.createElement('span'),line=document.createElement('span'),value=document.createElement('span'),unit=document.createElement('span'),quality=document.createElement('small');
   tile.type='button';tile.className='channel-tile';tile.dataset.channel=key;tile.dataset.quantity=r.quantity;tile.style.setProperty('--channel-hue',hue);
   const descriptor=state.device.components?.find(c=>c.id===r.componentId);
   const description=`${r.componentId} (${r.unit})${['charge','energy'].includes(r.quantity)?' · native counter':''}${descriptor?.temperatureKind?' · '+descriptor.temperatureKind:''}`;
   tile.setAttribute('aria-label',`${r.deviceId} / ${description}`);label.className='lab';label.textContent=r.componentId.replace(/-/g,' ');
   value.id=`channel-value-${this.channels.size}`;quality.id=`channel-quality-${this.channels.size}`;tile.setAttribute('aria-describedby',`${value.id} ${quality.id}`);
   value.dataset.value='';value.textContent='—';line.className='val';unit.className='un';unit.textContent=r.unit==='degC'?'°C':r.unit;quality.className='sr-only';line.append(value,unit);tile.append(label,line,quality);group.tiles.append(tile);
   const row=document.createElement('div');row.className='channel-choice';row.style.setProperty('--channel-hue',hue);
   const choose=(kind:string,defaultValue:boolean)=>{
    const wrapper=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=this.saved(key+':'+kind,defaultValue);input.setAttribute('aria-label',`${state.device.id} / ${r.componentId} ${kind}`);wrapper.append(input,document.createTextNode(kind==='tile'?`${r.componentId} (${r.unit})`:'curve'));row.append(wrapper);
    input.onchange=()=>{try{localStorage.setItem('bench:channel:'+key+':'+kind,input.checked?'1':'0');}catch{}this.render();};return input;
   };
   const tileChoice=choose('tile',true),curveChoice=choose('curve',defaultCurve);group.choices.append(row);
   tile.onclick=()=>{curveChoice.checked=!curveChoice.checked;curveChoice.dispatchEvent(new Event('change'));};
   this.channels.set(key,{tile,value,quality,tileChoice,curveChoice});
  }
 }
 sample(state:BenchDevice,mode:string){
  const record=mode==='live'||!state.componentId||[...state.actors.values()].some(a=>a.output==='on');
  this.recording.addBatch(state.device.id,state.readings,state.device.limits?.maxAgeMs??3000,record);
  if(record&&state.measurement){
   const m=state.measurement,statuses=moduleFor(state.device).status?.(m)??[];
   this.recording.addStatus(m,[...statuses,...[...state.actors.entries()].flatMap(([componentId,a])=>[
    {id:componentId+':output',value:a.output},
    ...(a.settings.voltageV===undefined?[]:[{id:componentId+':set-voltage',unit:'V',value:a.settings.voltageV}]),
    ...(a.settings.currentA===undefined?[]:[{id:componentId+':set-current',unit:'A',value:a.settings.currentA}])
   ])]);
  }
  if(state.error)this.recording.break(state.device.id);
  this.ensure(state);this.render();
 }
 render(){
  const series=[],visible=new Map<string,{total:number;fresh:number}>();
  for(const [key,view] of this.channels){
   const c=this.recording.channels.get(key),r=c.latest;
   const valid=this.api.connected&&!this.api.states.get(r.deviceId)?.error&&r.bootId===this.api.boot&&r.quality==='valid'&&Number.isFinite(r.value)&&r.ageMs+performance.now()-c.receivedAt<=c.maxAgeMs;
   view.tile.hidden=!view.tileChoice.checked;view.value.textContent=valid?r.value.toFixed(r.quantity==='current'?4:r.quantity==='temperature'?1:3):'—';view.quality.textContent=valid?'fresh':r.quality==='unavailable'?'unavailable':'stale / unavailable';
   const plotted=view.curveChoice.checked&&r.bootId===this.api.boot;
   view.tile.setAttribute('aria-pressed',String(plotted));view.tile.classList.toggle('unavailable',!valid);
   view.tile.title=`${view.tile.getAttribute('aria-label')} · ${view.quality.textContent} · ${plotted?'Hide':'Show'} curve`;
   view.tile.dataset.help='Click a value to show or hide its curve. Choose visible values under Display → Channels.';
   if(!view.tile.hidden){const count=visible.get(r.deviceId)??{total:0,fresh:0};count.total++;if(valid)count.fresh++;visible.set(r.deviceId,count);}
   if(!valid)this.recording.gap(c);
   if(plotted){
    series.push({key,unit:r.unit,color:getComputedStyle(view.tile).getPropertyValue('--channel-color').trim(),points:c.points});
   }
  }
  for(const [id,group] of this.groups){
   const count=visible.get(id);group.row.hidden=!count?.total;
   group.quality.textContent=!count?.total?'':count.fresh===count.total?'live':count.fresh?`${count.fresh}/${count.total} fresh`:'unavailable';
   group.heading.classList.toggle('has-error',!!count&&count.fresh<count.total);
  }
  for(const s of this.api.states.values()){
   let row=this.overview.get(s.device.id);
   if(!row){row=document.createElement('div');row.dataset.device=s.device.id;document.getElementById('deviceOverview')!.append(row);this.overview.set(s.device.id,row);}
   const outputs=[...s.actors.entries()].map(([id,a])=>`${id}: ${this.api.freshState(s)?a.output:'unknown'}`).join(', ');
   row.textContent=`${s.device.id} · ${outputs||'sensor'}${s.lease?' · reserved':s.owned?' · reservation uncertain':''}${s.busy?' · confirming':''}${s.uncertain?' · uncertain':''}${s.error?' · '+s.error:''}${s.controlError?' · '+s.controlError:''}`;
  }
  this.chart.setSeries(series);
 }
 clear(){this.recording.clear();this.render();}
}
