// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import {RequestQueue} from './request-queue.js';
import {Configuration,CoreApi,ComponentsApi,Dl24mApi} from '../../generated/src/index';
import type {Device,Measurements,Dl24mSettings,Operation,ActuatorState} from '../../generated/src/index';
export async function message(error:unknown):Promise<string>{
 if(error&&typeof error==='object'&&'response' in error){
  const response=(error as {response:Response}).response;
  try{const p=await response.clone().json();return p.detail||p.title||`HTTP ${response.status}`;}catch{return `HTTP ${response.status}`;}
 }
 return error instanceof Error?error.message:String(error);
}
const delay=(ms:number)=>new Promise(r=>setTimeout(r,ms));
function key(){return Array.from(crypto.getRandomValues(new Uint8Array(16)),b=>b.toString(16).padStart(2,'0')).join('');}
export class BenchAdapter {
 token='';devices:Device[]=[];device?:Device;boot='';lease='';connected=false;busy=false;uncertain=false;
 measurement?:Measurements;settings:Dl24mSettings={};actor?:ActuatorState;receivedAt=0;pollMs=1000;
 onChange=()=>{};onSample=(m:Measurements)=>{};onError=(m:string)=>{};
 private generation=0;private operationGeneration=0;private timer?:number;private heartbeat?:number;private heartbeatBusy=false;private settingsAt=0;
 private readonly requests=new RequestQueue();
 readonly config=new Configuration({basePath:location.origin,accessToken:()=>this.token,fetchApi:(input,init)=>{
  const generation=this.generation,path=new URL(String(input),location.origin).pathname;
  const stop=path.endsWith('/stop'),priority=stop?0:path.endsWith('/heartbeat')?1:init?.method!=='GET'||path.includes('/operations/')?2:3;
  return this.requests.run(async()=>{
   if(generation!==this.generation&&!stop)throw Error('Connection changed before request was sent');
   const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),4000);
   try{
    const response=await fetch(input,{...init,signal:controller.signal});
    // Keep the slot until the response body has arrived, not just its headers.
    await response.clone().arrayBuffer();return response;
   }catch(error){
    this.requests.cancelPending(Error('Connection lost before queued request was sent'));
    this.lose('Connection lost: HTTP request failed; queued commands cancelled');
    throw error;
   }finally{clearTimeout(timer);}
  },priority);
 }});
 readonly core=new CoreApi(this.config);readonly components=new ComponentsApi(this.config);readonly dl=new Dl24mApi(this.config);
 get sink(){return this.device?.components?.find(c=>c.kind==='sink')?.id;}
 get fresh(){return !!this.measurement&&this.measurement.quality==='valid'&&this.measurement.ageMs+performance.now()-this.receivedAt<=(this.device?.limits?.maxAgeMs??0);}
 get writable(){return this.connected&&!!this.lease&&!this.busy&&!this.uncertain&&this.fresh&&this.actor?.state==='ready'&&this.actor.output!=='unknown';}
 get off(){return this.writable&&this.actor?.output==='off';}
 get canOn(){return this.off&&this.settings.mode===0;}
 supports(action:string){return this.device?.components?.some(c=>c.id===this.sink&&c.actions.includes(action))??false;}
 private cancelPolling(){this.generation++;clearTimeout(this.timer);}
 private stopHeartbeat(){clearInterval(this.heartbeat);this.heartbeat=undefined;}
 private lose(text:string){this.cancelPolling();this.stopHeartbeat();this.connected=false;this.lease='';this.uncertain=true;this.actor=undefined;this.onError(text);this.onChange();}
 async connect(token:string){
  if(this.lease)await this.release();
  this.cancelPolling();this.token=token;this.connected=false;this.uncertain=false;this.measurement=undefined;this.actor=undefined;this.onChange();
  const node=await this.core.getNode(),list=await this.core.listDevices();
  this.devices=list.devices.filter(d=>d.driver==='dl24m');this.boot=node.bootId;
  if(!this.devices.length){this.device=undefined;this.onChange();throw Error('No DL24M device configured on this node');}
  this.device=this.devices.find(d=>d.id===this.device?.id)??this.devices[0];
  if(!this.sink)throw Error('DL24M sink component unavailable');
  this.settingsAt=0;this.connected=true;await this.poll(this.generation);this.onChange();
 }
 private async poll(generation:number){
  try{
   const id=this.device!.id,componentId=this.sink!;
   // A browser's parallel reads can exhaust the node's shared socket budget
   // (HTTP, AP DNS, mDNS and instrument TCP). Reuse connections sequentially.
   const node=await this.core.getNode(),requestedAt=performance.now();
   const m=await this.core.getMeasurements({deviceId:id});
   const a=await this.components.getActuatorState({deviceId:id,componentId});
   const settings=!this.settingsAt||performance.now()-this.settingsAt>=5000
    ?await this.dl.getDl24mSettings({deviceId:id}):undefined;
   if(generation!==this.generation)return;
   if(node.bootId!==this.boot||m.bootId&&m.bootId!==this.boot){this.lose('ESP restarted: reservation invalidated. Reconnect to start a new session.');return;}
   this.measurement={...m,bootId:this.boot,ageMs:m.ageMs+performance.now()-requestedAt};this.receivedAt=performance.now();this.actor=a;
   if(settings){this.settings=settings;this.settingsAt=performance.now();}
   this.onSample(this.measurement);this.onChange();
  }catch(e){if(generation===this.generation)this.lose('Connection lost: '+await message(e));return;}
  if(generation===this.generation&&this.connected)this.timer=window.setTimeout(()=>void this.poll(generation),this.pollMs);
 }
 async select(id:string){
  if(this.busy)throw Error('Wait for the pending operation before switching devices');
  if(this.lease)await this.release();
  const next=this.devices.find(d=>d.id===id);if(!next)throw Error('Unknown DL24M device');
  this.cancelPolling();this.device=next;this.actor=undefined;this.measurement=undefined;this.settings={};this.settingsAt=0;
  this.onChange();if(this.connected)await this.poll(this.generation);
 }
 async reserve(){
  if(!this.connected||!this.device||!this.fresh||this.uncertain)throw Error('Reconnect and wait for fresh measurements before reserving');
  if(this.lease)throw Error('Device already reserved by this page');
  const l=await this.core.createLease({leaseRequest:{deviceIds:[this.device.id],ttlMs:10000}});
  if(l.state!=='active')throw Error('Reservation is not active');
  this.lease=l.id;this.heartbeat=window.setInterval(()=>void this.beat(),1000);this.onChange();
 }
 private async beat(){
  if(this.heartbeatBusy||!this.lease)return;
  const lease=this.lease;this.heartbeatBusy=true;
  try{const l=await this.core.heartbeatLease({leaseId:lease,stopRequest:{}});if(l.state!=='active')throw Error('Lease is not active');}
  catch(e){if(this.lease===lease)this.lose('Lease lost: '+await message(e));}
  finally{this.heartbeatBusy=false;}
 }
 private params(){
  if(!this.writable||!this.device)throw Error('A reservation, fresh measurements and confirmed device state are required');
  return {deviceId:this.device.id,componentId:this.sink!,xLabLease:this.lease,idempotencyKey:key()};
 }
 private async perform(send:()=>Promise<Operation>,stop=false){
  if(this.busy&&!stop)throw Error('Wait for the pending operation');
  const operationGeneration=++this.operationGeneration,boot=this.boot;
  this.busy=true;this.onChange();
  try{
   let o=await send();const deadline=performance.now()+12000;
   while(o.status==='running'||o.status==='pending'){
    if(operationGeneration!==this.operationGeneration)throw Error('Operation superseded by STOP');
    if(performance.now()>deadline)throw Error('Operation uncertain: query its result before repeating');
    await delay(150);o=await this.core.getOperation({operationId:o.id});
   }
   if(operationGeneration!==this.operationGeneration)throw Error('Operation superseded by STOP');
   if(boot!==this.boot||!this.connected&&!stop)throw Error('Connection changed while confirming the operation');
   if(o.status!=='succeeded'||o.restorePending||o.finalOutput==='unknown')throw Error(`${o.status}: ${o.stage}; output ${o.finalOutput}; restore pending ${o.restorePending}`);
   if(stop&&o.finalOutput!=='off')throw Error('STOP did not confirm OFF');
   const actor=await this.components.getActuatorState({deviceId:this.device!.id,componentId:this.sink!});
   const settings=await this.dl.getDl24mSettings({deviceId:this.device!.id});
   if(operationGeneration!==this.operationGeneration)return o;
   this.actor=actor;this.settings=settings;this.settingsAt=performance.now();
   if(stop&&actor.output!=='off')throw Error('OFF readback was not confirmed');
   // Only an explicit confirmed STOP can clear an uncertain command latch.
   if(stop)this.uncertain=false;
   return o;
  }catch(e){if(operationGeneration===this.operationGeneration)this.uncertain=true;throw e;}
  finally{if(operationGeneration===this.operationGeneration){this.busy=false;this.onChange();}}
 }
 async current(currentA:number){const p=this.params();if(!this.supports('current')||this.settings.mode!==0)throw Error('CC current is unavailable');return this.perform(()=>this.components.setComponentCurrent({...p,currentRequest:{currentA}}));}
 async enable(){if(!this.canOn||!this.supports('output'))throw Error('ON requires fresh CC state and confirmed OFF');const p=this.params();return this.perform(()=>this.components.setComponentOutput({...p,outputRequest:{enabled:true}}));}
 async stop(){this.requests.cancelPending(Error('Cancelled by priority STOP'),job=>job.priority===2);if(!this.device)throw Error('Select a DL24M device first');const deviceId=this.device.id;return this.perform(()=>this.core.stopDevice({deviceId,idempotencyKey:key(),stopRequest:{reason:'DL24M Bench STOP'}}),true);}
 async settingsWrite(settings:Dl24mSettings){if(!this.off)throw Error('Settings require a reservation and confirmed OFF');const p=this.params();return this.perform(()=>this.dl.setDl24mSettings({...p,dl24mSettings:settings}));}
 async duration(timeLimitS:number){if(!this.off||!this.supports('duration'))throw Error('Timer requires confirmed OFF and backend support');const p=this.params();return this.perform(()=>this.components.setComponentDuration({...p,durationRequest:{timeLimitS}}));}
 async cutoff(cutoffVoltageV:number){if(!this.off||!this.supports('cutoff'))throw Error('Cutoff requires confirmed OFF and backend support');const p=this.params();return this.perform(()=>this.components.setComponentCutoff({...p,cutoffRequest:{cutoffVoltageV}}));}
 async resetCounters(){if(!this.off)throw Error('Counter reset requires confirmed OFF');const p=this.params();return this.perform(()=>this.dl.resetDl24mCounters({...p,stopRequest:{reason:'DL24M Bench explicit counter reset'}}));}
 async release(){
  if(!this.lease)return;
  const lease=this.lease;await this.stop();
  await this.core.stopLease({leaseId:lease,stopRequest:{reason:'DL24M Bench release after confirmed OFF'}});
  this.lease='';this.stopHeartbeat();this.onChange();
 }
 unload(){
  this.cancelPolling();this.stopHeartbeat();
  if(this.lease&&this.device)void this.core.stopDevice({deviceId:this.device.id,idempotencyKey:key(),stopRequest:{reason:'Bench page closed'}},{keepalive:true}).catch(()=>{});
 }
}
