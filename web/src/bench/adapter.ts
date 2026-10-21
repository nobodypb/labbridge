// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import {RequestQueue} from './request-queue.js';
import {UiAuth} from '../auth';
import {Configuration,CoreApi,ComponentsApi} from '../../generated/src/index';
import type {Device,Measurements,Operation,ActuatorState,SensorReading} from '../../generated/src/index';
import {moduleFor} from './modules';
import type {BenchSettings} from './modules/types';
export async function message(error:unknown):Promise<string>{
 if(error&&typeof error==='object'&&'response' in error){
  const response=(error as {response:Response}).response;
  try{const p=await response.clone().json();return p.detail||p.title||`HTTP ${response.status}`;}catch{return `HTTP ${response.status}`;}
 }
 return error instanceof Error?error.message:String(error);
}
const delay=(ms:number)=>new Promise(r=>setTimeout(r,ms));
function key(){return Array.from(crypto.getRandomValues(new Uint8Array(16)),b=>b.toString(16).padStart(2,'0')).join('');}
export interface BenchDevice {
 device:Device;componentId:string;lease:string;owned:boolean;busy:boolean;uncertain:boolean;error:string;controlError:string;
 measurement?:Measurements;readings:SensorReading[];actors:Map<string,ActuatorState>;settings:BenchSettings;
 receivedAt:number;settingsAt:number;operationGeneration:number;draft:Record<string,string>;
}
export class BenchAdapter {
 readonly auth=new UiAuth();
 devices:Device[]=[];states=new Map<string,BenchDevice>();device?:Device;boot='';connected=false;connecting=false;pollMs=1000;shuttingDown=false;
 onChange=()=>{};onSample=(state:BenchDevice)=>{};onError=(m:string,deviceId?:string)=>{};
 private generation=0;private timer?:number;private heartbeat?:number;private heartbeatBusy=false;
 private operationOwners=new Map<string,string>();
 private readonly requests=new RequestQueue();
 readonly config=new Configuration({basePath:location.origin,fetchApi:(input,init)=>{
  const generation=this.generation,path=new URL(String(input),location.origin).pathname;
  const stop=path.endsWith('/stop'),heartbeat=path.endsWith('/heartbeat');
  const deviceId=path.startsWith('/api/v1/devices/')?decodeURIComponent(path.split('/')[4]):this.operationOwners.get(path.split('/')[4]);
  const priority=stop?0:heartbeat?1:init?.method!=='GET'||path.includes('/operations/')?2:3;
  return this.requests.run(async()=>{
   if(generation!==this.generation&&!stop)throw Error('Connection changed before request was sent');
   const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),4000);
   try{
    const response=await this.auth.fetch(input,{...init,signal:controller.signal});
    await response.clone().arrayBuffer();
    if(heartbeat){
     const lease=path.split('/')[4],owner=[...this.states.values()].find(s=>s.lease===lease);
     let active=false;
     try{const result=await response.clone().json();active=response.ok&&result.state==='active'&&result.id===lease;}catch{}
     // Invalidate inside the queue slot: consumers see HTTP errors only after
     // the slot has drained, which would otherwise let a queued ON escape.
     if(owner&&!active)this.invalidateLease(owner,`Lease lost: heartbeat inactive (HTTP ${response.status})`);
    }
    return response;
   }catch(error){
    this.requests.cancelPending(Error('Connection lost before queued request was sent'));
    this.lose('Connection lost: HTTP request failed; queued commands cancelled');throw error;
   }finally{clearTimeout(timer);}
  },priority,deviceId);
 }});
 readonly core=new CoreApi(this.config);readonly components=new ComponentsApi(this.config);
 constructor(){this.auth.core=new CoreApi(new Configuration({basePath:location.origin,fetchApi:this.auth.fetch}));}
 get selected(){return this.device?this.states.get(this.device.id):undefined;}
 get actors(){return this.device?.components?.filter(c=>c.kind!=='sensor')??[];}
 get module(){return this.device?moduleFor(this.device):undefined;}
 get componentId(){return this.selected?.componentId;}
 get lease(){return this.selected?.lease??'';}
 get busy(){return this.selected?.busy??false;}
 get uncertain(){return this.selected?.uncertain??false;}
 get actor(){return this.selected?.actors.get(this.componentId??'');}
 get measurement(){return this.selected?.measurement;}
 get settings(){return this.selected?.settings??{};}
 get reserved(){return [...this.states.values()].filter(s=>s.owned);}
 freshState(s:BenchDevice){const m=s.measurement;return this.connected&&!this.connecting&&!!m&&m.quality==='valid'&&m.ageMs+performance.now()-s.receivedAt<=(s.device.limits?.maxAgeMs??3000);}
 get fresh(){return !!this.selected&&this.freshState(this.selected);}
 get writable(){return this.auth.state.authenticated&&!this.auth.loggingOut&&!this.shuttingDown&&this.connected&&!!this.lease&&!this.busy&&!this.uncertain&&this.fresh&&this.actor?.state==='ready'&&this.actor.output!=='unknown';}
 get off(){return this.writable&&this.actor?.output==='off';}
 get canOn(){return this.off&&!!this.module?.canControl(this.settings);}
 supports(action:string){return this.device?.components?.some(c=>c.id===this.componentId&&c.actions.includes(action))??false;}
 private cancelPolling(){this.generation++;clearTimeout(this.timer);}
 private stopHeartbeat(){clearInterval(this.heartbeat);this.heartbeat=undefined;}
 private lose(text:string){
  this.cancelPolling();this.stopHeartbeat();this.connected=false;this.connecting=false;
  for(const s of this.states.values()){s.lease='';s.uncertain=true;s.actors.clear();s.operationGeneration++;s.busy=false;}
  this.onError(text);this.onChange();
 }
 endSession(){this.requests.cancelPending(Error('Session ended; queued commands cancelled'));this.lose('Bench control disconnected. Verify outputs OFF; unconfirmed reservations expire on the bridge.');}
 async connect(){
  if(!this.auth.state.authenticated||this.auth.loggingOut)throw Error('Sign in before connecting');
  // Explicit rediscovery never abandons known ownership silently.
  if(this.reserved.length&&this.connected)await this.releaseAll();
  this.cancelPolling();const generation=this.generation;this.connected=false;this.connecting=true;this.onChange();
  try{
  const node=await this.core.getNode(),list=await this.core.listDevices();
  if(generation!==this.generation)throw Error('Session changed during discovery');
  this.devices=list.devices;this.boot=node.bootId;this.states.clear();
  if(!this.devices.length){this.device=undefined;this.connecting=false;this.onChange();throw Error('No devices configured on this node');}
  for(const d of this.devices)this.states.set(d.id,{device:d,componentId:d.components?.find(c=>c.kind!=='sensor')?.id??'',lease:'',owned:false,busy:false,uncertain:false,error:'',controlError:'',readings:[],actors:new Map(),settings:{},receivedAt:0,settingsAt:0,operationGeneration:0,draft:{}});
  this.device=this.devices.find(d=>d.id===this.device?.id)??this.devices[0];this.connected=true;
  this.heartbeat=window.setInterval(()=>void this.beat(),1000);
  await this.poll(generation);
  if(generation!==this.generation||!this.connected)throw Error('Connection changed during discovery');
  }finally{if(generation===this.generation){this.connecting=false;this.onChange();}}
 }
 private async poll(generation:number){
  try{
   const node=await this.core.getNode();
   if(generation!==this.generation)return;
   if(node.bootId!==this.boot){this.lose('ESP restarted: reservations invalidated. Reconnect to start a new session.');return;}
   for(const s of this.states.values()){
    if(generation!==this.generation||!this.connected)return;
    try{
     const deviceId=s.device.id,hasActors=s.device.components?.some(c=>c.kind!=='sensor'),requestedAt=performance.now();
     // Read the aggregate snapshot only for actors: generic meters need only readings.
     const m=hasActors?await this.core.getMeasurements({deviceId}):undefined;
     if(generation!==this.generation)return;
     if(m?.bootId&&m.bootId!==this.boot){this.lose('ESP restarted: reservations invalidated. Reconnect to start a new session.');return;}
     if(m){s.measurement={...m,bootId:this.boot,ageMs:m.ageMs+performance.now()-requestedAt};s.receivedAt=performance.now();}
     const actors=s.device.components?.filter(c=>c.kind!=='sensor')??[];
     for(const c of actors){
      const a=await this.components.getActuatorState({deviceId,componentId:c.id});
      if(generation!==this.generation)return;s.actors.set(c.id,a);
     }
     const at=performance.now(),samples=await this.components.getSensorReadings({deviceId});
     if(generation!==this.generation)return;
     if(samples.bootId!==this.boot||samples.readings.some(r=>r.bootId!==this.boot)){this.lose('ESP restarted: reservations invalidated. Reconnect to start a new session.');return;}
     s.readings=samples.readings.map(r=>({...r,ageMs:r.ageMs+performance.now()-at}));
     // Discover late-appearing sensor descriptors without requiring a new session.
     for(const r of s.readings)if(!s.device.components?.some(c=>c.id===r.componentId)){
      (s.device.components??=[]).push({id:r.componentId,kind:'sensor',actions:[],quantity:r.quantity,unit:r.unit});
     }
     const module=moduleFor(s.device);
     if(module.readSettings&&(!s.settingsAt||performance.now()-s.settingsAt>=5000)){
      const settings=await module.readSettings(this.config,deviceId);
      if(generation!==this.generation)return;s.settings=settings;s.settingsAt=performance.now();
     }
     s.error='';this.onSample(s);this.onChange();
    }catch(e){
     if(generation!==this.generation)return;
     s.error=await message(e);s.measurement=undefined;s.readings=[];s.actors.clear();
     this.onSample(s);this.onError(`${s.device.id}: ${s.error}`,s.device.id);this.onChange();
    }
   }
  }catch(e){if(generation===this.generation)this.lose('Connection lost: '+await message(e));return;}
  if(generation===this.generation&&this.connected)this.timer=window.setTimeout(()=>void this.poll(generation),this.pollMs);
 }
 async select(id:string){
  if(!this.auth.state.authenticated||this.auth.loggingOut)throw Error('Sign in before selecting a device');
  const next=this.devices.find(d=>d.id===id);if(!next)throw Error('Unknown device');
  this.device=next;this.onChange();
 }
 selectComponent(componentId:string){
  if(!this.selected||!this.actors.some(c=>c.id===componentId))throw Error('Unknown actor component');
  this.selected.componentId=componentId;this.onChange();
 }
 async reserve(){
  const s=this.selected;
  if(!this.auth.state.authenticated||this.auth.loggingOut)throw Error('Sign in before reserving');
  if(this.shuttingDown||!s?.componentId||!this.connected||!this.freshState(s)||s.uncertain)throw Error('Reconnect and wait for fresh measurements before reserving');
  if(s.lease)throw Error('Device already reserved by this page');
  const generation=this.generation,l=await this.core.createLease({leaseRequest:{deviceIds:[s.device.id],ttlMs:10000}});
  if(generation!==this.generation||this.auth.loggingOut)throw Error('Session changed while reserving; verify outputs OFF and allow lease expiry');
  if(l.state!=='active')throw Error('Reservation is not active');
  s.lease=l.id;s.owned=true;this.onChange();
 }
 private invalidateLease(s:BenchDevice,text:string){
  s.lease='';s.uncertain=true;s.controlError=text;s.operationGeneration++;s.busy=false;
  this.requests.cancelPending(Error('Lease lost before queued command was sent'),job=>job.priority===2&&job.deviceId===s.device.id);
  this.onError(`${s.device.id}: ${text}`,s.device.id);this.onChange();
 }
 private async beat(){
  if(this.heartbeatBusy||!this.connected)return;this.heartbeatBusy=true;
  try{for(const s of this.states.values()){
   const lease=s.lease;if(!lease)continue;
   try{const l=await this.core.heartbeatLease({leaseId:lease,stopRequest:{}});if(l.state!=='active')throw Error('Lease is not active');}
   catch(e){if(s.lease===lease)this.invalidateLease(s,'Lease lost: '+await message(e));}
  }}finally{this.heartbeatBusy=false;}
 }
 private params(){
  const s=this.selected;if(!this.writable||!s)throw Error('A reservation, fresh measurements and confirmed device state are required');
  return {s,params:{deviceId:s.device.id,componentId:s.componentId,xLabLease:s.lease,idempotencyKey:key()}};
 }
 private async perform(s:BenchDevice,componentId:string,send:()=>Promise<Operation>,stop=false){
  if(s.busy&&!stop)throw Error('Wait for the pending operation');
  const operationGeneration=++s.operationGeneration,boot=this.boot,generation=this.generation;
  s.busy=true;this.onChange();let operationId='';
  try{
   let o=await send();operationId=o.id;this.operationOwners.set(o.id,s.device.id);const deadline=performance.now()+12000;
   const check=()=>{if(operationGeneration!==s.operationGeneration)throw Error('Operation superseded by STOP');if(boot!==this.boot||generation!==this.generation)throw Error('Connection changed while confirming the operation');};
   while(o.status==='running'||o.status==='pending'){
    check();if(performance.now()>deadline)throw Error('Operation uncertain: query its result before repeating');
    await delay(150);check();o=await this.core.getOperation({operationId:o.id});
   }
   check();if(o.deviceId!==s.device.id)throw Error('Operation device identity mismatch');
   if(o.status!=='succeeded'||o.restorePending||o.finalOutput==='unknown')throw Error(`${o.status}: ${o.stage}; output ${o.finalOutput}; restore pending ${o.restorePending}`);
   if(stop&&o.finalOutput!=='off')throw Error('STOP did not confirm OFF');
   // Device STOP covers all actors; refresh all of them before claiming OFF.
   const ids=stop?(s.device.components?.filter(c=>c.kind!=='sensor').map(c=>c.id)??[]):[componentId];
   for(const id of ids){const actor=await this.components.getActuatorState({deviceId:s.device.id,componentId:id});check();s.actors.set(id,actor);if(stop&&actor.output!=='off')throw Error('OFF readback was not confirmed');}
   const module=moduleFor(s.device);
   if(module.readSettings){const settings=await module.readSettings(this.config,s.device.id);check();s.settings=settings;s.settingsAt=performance.now();}
   if(stop){s.uncertain=false;s.controlError='';}return o;
  }catch(e){if(operationGeneration===s.operationGeneration){s.uncertain=true;s.controlError=await message(e);}throw e;}
  finally{this.operationOwners.delete(operationId);if(operationGeneration===s.operationGeneration){s.busy=false;this.onChange();}}
 }
 async current(currentA:number){const {s,params:p}=this.params();if(!this.supports('current')||!moduleFor(s.device).canControl(s.settings))throw Error('Current control is unavailable');return this.perform(s,p.componentId,()=>this.components.setComponentCurrent({...p,currentRequest:{currentA}}));}
 async voltage(voltageV:number){const {s,params:p}=this.params();if(!this.supports('voltage'))throw Error('Voltage control is unavailable');return this.perform(s,p.componentId,()=>this.components.setComponentVoltage({...p,voltageRequest:{voltageV}}));}
 async enable(){if(!this.canOn||!this.supports('output'))throw Error('ON requires fresh state and confirmed OFF');const {s,params:p}=this.params();return this.perform(s,p.componentId,()=>this.components.setComponentOutput({...p,outputRequest:{enabled:true}}));}
 async stop(deviceId=this.device?.id){
  const s=deviceId?this.states.get(deviceId):undefined;if(!s?.componentId)throw Error('Select an actor device first');
  this.requests.cancelPending(Error('Cancelled by priority STOP'),job=>job.priority===2&&job.deviceId===deviceId);
  return this.perform(s,s.componentId,()=>this.core.stopDevice({deviceId:s.device.id,idempotencyKey:key(),stopRequest:{reason:'Bench STOP'}}),true);
 }
 private async eachOwned(action:(s:BenchDevice)=>Promise<unknown>){
  if(this.shuttingDown)throw Error('Shutdown already in progress');
  this.shuttingDown=true;this.onChange();
  const owned=[...this.reserved],errors:string[]=[];
  try{
   const ids=new Set(owned.map(s=>s.device.id));
   this.requests.cancelPending(Error('Cancelled by shutdown'),job=>job.priority===2&&ids.has(job.deviceId??''));
   // Submit every priority STOP before awaiting confirmations. The shared
   // queue still allows only one HTTP request at a time.
   const results=await Promise.allSettled(owned.map(action));
   for(let i=0;i<results.length;i++){const result=results[i];if(result.status==='rejected')errors.push(`${owned[i].device.id}: ${await message(result.reason)}`);}
   if(errors.length)throw Error(errors.join('; '));
  }finally{this.shuttingDown=false;this.onChange();}
 }
 async stopAll(){await this.eachOwned(s=>this.stop(s.device.id));}
 async settingsWrite(settings:BenchSettings){
  if(!this.off)throw Error('Settings require a reservation and confirmed OFF');const {s,params:p}=this.params(),module=moduleFor(s.device);
  if(!module.writeSettings)throw Error('Device settings unavailable');return this.perform(s,p.componentId,()=>module.writeSettings!(this.config,p,settings));
 }
 async duration(timeLimitS:number){if(!this.off||!this.supports('duration'))throw Error('Timer requires confirmed OFF and backend support');const {s,params:p}=this.params();return this.perform(s,p.componentId,()=>this.components.setComponentDuration({...p,durationRequest:{timeLimitS}}));}
 async cutoff(cutoffVoltageV:number){if(!this.off||!this.supports('cutoff'))throw Error('Cutoff requires confirmed OFF and backend support');const {s,params:p}=this.params();return this.perform(s,p.componentId,()=>this.components.setComponentCutoff({...p,cutoffRequest:{cutoffVoltageV}}));}
 async resetCounters(){if(!this.off)throw Error('Counter reset requires confirmed OFF');const {s,params:p}=this.params(),module=moduleFor(s.device);if(!module.resetCounters)throw Error('Counter reset unavailable');return this.perform(s,p.componentId,()=>module.resetCounters!(this.config,p));}
 async release(deviceId=this.device?.id){
  const s=deviceId?this.states.get(deviceId):undefined;if(!s?.owned)return;
  const lease=s.lease;await this.stop(s.device.id);
  if(lease)await this.core.stopLease({leaseId:lease,stopRequest:{reason:'Bench release after confirmed OFF'}});
  s.lease='';s.owned=false;this.onChange();
 }
 async releaseAll(){await this.eachOwned(s=>this.release(s.device.id));}
 unload(){
  const owned=[...this.reserved];this.cancelPolling();this.stopHeartbeat();
  this.requests.cancelPending(Error('Page closed'));
  for(const s of owned)void this.core.stopDevice({deviceId:s.device.id,idempotencyKey:key(),stopRequest:{reason:'Bench page closed'}},{keepalive:true}).catch(()=>{});
 }
}
