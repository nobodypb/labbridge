// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import {RpcStream} from './stream.js';
import {RequestQueue} from './request-queue.js';
import {SampleClock} from './sample-clock.js';
import {Configuration,CoreApi,ComponentsApi,Dl24mApi} from '../../generated/src/index';
import {DeviceFromJSON,MeasurementsFromJSON,Dl24mSettingsFromJSON,OperationFromJSON,ActuatorStateFromJSON} from '../../generated/src/index';
import type {Device,Measurements,Dl24mSettings,Operation,ActuatorState} from '../../generated/src/index';
export async function message(error:unknown):Promise<string>{
 if(error&&typeof error==='object'&&'response' in error){
  const response=(error as {response:Response}).response;
  try{const p=await response.clone().json();return p.detail||p.title||`HTTP ${response.status}`;}catch{return `HTTP ${response.status}`;}
 }
 return error instanceof Error?error.message:String(error);
}

function key(){return Array.from(crypto.getRandomValues(new Uint8Array(16)),b=>b.toString(16).padStart(2,'0')).join('');}
export class BenchAdapter {
 token='';devices:Device[]=[];device?:Device;boot='';lease='';connected=false;busy=false;uncertain=false;
 measurement?:Measurements;settings:Dl24mSettings={};actor?:ActuatorState;receivedAt=0;pollMs=1000;
 onChange=()=>{};onSample=(m:Measurements)=>{};onError=(m:string)=>{};onGap=()=>{};
 readonly stream=new RpcStream(`${location.protocol==='https:'?'wss:':'ws:'}//${location.host}/api/v1/ws`);
 private readonly sampleClock=new SampleClock();
 private operations=new Map<string,Operation>();
 private operationWaiters=new Map<string,{resolve:(o:Operation)=>void;reject:(e:Error)=>void;timer:ReturnType<typeof setTimeout>}>();
 constructor(){
  this.stream.onSnapshot=(event:any)=>this.snapshot(event);this.stream.onGap=()=>this.onGap();
  this.stream.onLoss=(error:Error)=>{if(this.connected)this.lose(error.message);};
 }
 private generation=0;private operationGeneration=0;private heartbeat?:number;private heartbeatBusy=false;
 private readonly requests=new RequestQueue();
 readonly config=new Configuration({basePath:location.origin,accessToken:()=>this.token,fetchApi:(input,init)=>{
  const generation=this.generation,path=new URL(String(input),location.origin).pathname;
  const stop=path.endsWith('/stop'),priority=stop?0:path.endsWith('/heartbeat')?1:init?.method!=='GET'||path.includes('/operations/')?2:3;
  return this.requests.run(async()=>{
   if(generation!==this.generation&&!stop)throw Error('Connection changed before request was sent');
   try{return await this.stream.fetch(String(input),init);}
   catch(error){
    this.requests.cancelPending(Error('Connection lost before queued request was sent'));
    this.lose('Connection lost: WebSocket request failed; queued commands cancelled');throw error;
   }
  },priority);
 }});
 readonly core=new CoreApi(this.config);readonly components=new ComponentsApi(this.config);readonly dl=new Dl24mApi(this.config);
 get sink(){return this.device?.components?.find(c=>c.kind==='sink')?.id;}
 get fresh(){return !!this.measurement&&this.measurement.quality==='valid'&&this.measurement.ageMs+performance.now()-this.receivedAt<=(this.device?.limits?.maxAgeMs??0);}
 get writable(){return this.connected&&!!this.lease&&!this.busy&&!this.uncertain&&this.fresh&&this.actor?.state==='ready'&&this.actor.output!=='unknown';}
 get off(){return this.writable&&this.actor?.output==='off';}
 get mode(){return (['cc','cv','cr','cp'] as const)[this.settings.mode??-1];}
 get modeSupported(){return this.mode==='cc'||this.device?.features?.some(f=>f.id===this.mode&&f.status==='supported');}
 get canOn(){return this.off&&this.modeSupported;}
 supports(action:string){return this.device?.components?.some(c=>c.id===this.sink&&c.actions.includes(action))??false;}
 private invalidate(){this.generation++;}
 private stopHeartbeat(){clearInterval(this.heartbeat);this.heartbeat=undefined;}
 private lose(text:string){this.invalidate();this.stopHeartbeat();this.connected=false;this.lease='';this.uncertain=true;this.actor=undefined;this.stream.close();for(const w of this.operationWaiters.values()){clearTimeout(w.timer);w.reject(Error(text));}this.operationWaiters.clear();this.onError(text);this.onChange();}
 async connect(token:string){
  if(this.lease)await this.release();
  this.invalidate();this.token=token;this.connected=false;this.uncertain=false;this.measurement=undefined;this.actor=undefined;this.onChange();
  this.boot=await this.stream.open(token);
  const node=await this.core.getNode(),list=await this.core.listDevices();
  this.devices=list.devices.filter(d=>d.driver==='dl24m');this.boot=node.bootId;
  if(!this.devices.length){this.device=undefined;this.onChange();throw Error('No DL24M device configured on this node');}
  this.device=this.devices.find(d=>d.id===this.device?.id)??this.devices[0];
  if(!this.sink)throw Error('DL24M sink component unavailable');
  this.connected=true;this.sampleClock.start(performance.now());await this.stream.subscribe(this.device.id,this.pollMs);await this.initialSnapshot();this.onChange();
 }
 private snapshot(event:any){
  if(event.boot_id!==this.boot){this.lose('ESP restarted: reservation invalidated. Reconnect to start a new session.');return;}
  const body=event.body;
  for(const raw of body.operations??[]){
   const o=OperationFromJSON(raw);this.operations.set(o.id,o);
   if(this.operations.size>16)this.operations.delete(this.operations.keys().next().value!);
   if(o.status!=='running'&&o.status!=='pending'){
    const waiter=this.operationWaiters.get(o.id);if(waiter){clearTimeout(waiter.timer);this.operationWaiters.delete(o.id);waiter.resolve(o);}
   }
  }
  if(body.device_id!==this.device?.id)return;
  if(body.devices){const list=body.devices.map(DeviceFromJSON);this.devices=list.filter((d:Device)=>d.driver==='dl24m');this.device=this.devices.find(d=>d.id===this.device?.id)??this.device;}
  const actor=body.actors?.find((a:any)=>a.component_id===this.sink);if(actor)this.actor=ActuatorStateFromJSON(actor);
  if(body.details?.settings){this.settings=Dl24mSettingsFromJSON(body.details.settings);}
  if(body.measurements){this.receivedAt=performance.now();const m:Measurements=this.sampleClock.adjust(MeasurementsFromJSON(body.measurements),this.receivedAt);this.measurement=m;this.onSample(m);}
  this.onChange();
 }
 private async initialSnapshot(){
  const deadline=performance.now()+4000;
  while(!this.measurement||!this.actor){
   if(!this.connected)throw Error('Connection lost before initial snapshot');
   if(performance.now()>deadline){this.lose('Connection lost: no initial snapshot');throw Error('Initial snapshot timed out');}
   await new Promise(r=>setTimeout(r,25));
  }
 }
 private waitOperation(o:Operation):Promise<Operation>{
  const known=this.operations.get(o.id)??o;
  if(known.status!=='running'&&known.status!=='pending')return Promise.resolve(known);
  return new Promise((resolve,reject)=>{
   const timer=setTimeout(()=>{this.operationWaiters.delete(o.id);reject(Error('Operation uncertain: result event timed out; inspect before repeating'));},12000);
   this.operationWaiters.set(o.id,{resolve,reject,timer});
  });
 }
 async interval(ms:number){this.pollMs=ms;if(this.connected&&this.device)await this.stream.subscribe(this.device.id,ms);}

 async select(id:string){
  if(this.busy)throw Error('Wait for the pending operation before switching devices');
  if(this.lease)await this.release();
  const next=this.devices.find(d=>d.id===id);if(!next)throw Error('Unknown DL24M device');
  this.invalidate();this.device=next;this.actor=undefined;this.measurement=undefined;this.settings={};
  this.onChange();if(this.connected){this.sampleClock.start(performance.now());await this.stream.subscribe(id,this.pollMs);await this.initialSnapshot();}
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
   const o=await this.waitOperation(await send());
   if(operationGeneration!==this.operationGeneration)throw Error('Operation superseded by STOP');
   if(boot!==this.boot||!this.connected&&!stop)throw Error('Connection changed while confirming the operation');
   if(o.status!=='succeeded'||o.restorePending||o.finalOutput==='unknown')throw Error(`${o.status}: ${o.stage}; output ${o.finalOutput}; restore pending ${o.restorePending}`);
   if(stop&&o.finalOutput!=='off')throw Error('STOP did not confirm OFF');
   const actor=await this.components.getActuatorState({deviceId:this.device!.id,componentId:this.sink!});
   const settings=await this.dl.getDl24mSettings({deviceId:this.device!.id});
   if(operationGeneration!==this.operationGeneration)return o;
   this.actor=actor;this.settings=settings;
   if(stop&&actor.output!=='off')throw Error('OFF readback was not confirmed');
   // Only an explicit confirmed STOP can clear an uncertain command latch.
   if(stop)this.uncertain=false;
   return o;
  }catch(e){if(operationGeneration===this.operationGeneration)this.uncertain=true;throw e;}
  finally{if(operationGeneration===this.operationGeneration){this.busy=false;this.onChange();}}
 }
 async current(currentA:number){const p=this.params();if(!this.supports('current')||this.settings.mode!==0)throw Error('CC current is unavailable');return this.perform(()=>this.components.setComponentCurrent({...p,currentRequest:{currentA}}));}
 async modeWrite(mode:'cc'|'cv'|'cr'|'cp'){if(!this.off||!this.device?.features?.some(f=>f.id===mode&&f.status==='supported'))throw Error('Mode is not supported or OFF is not confirmed');const p=this.params();return this.perform(()=>this.dl.setDl24mMode({...p,dl24mModeRequest:{mode}}));}
 async target(value:number){if(this.mode==='cc')return this.current(value);if(!this.off||!this.modeSupported||!this.mode)throw Error('Target requires supported mode and confirmed OFF');const p=this.params(),mode=this.mode,unit=({cv:'V',cr:'Ohm',cp:'W'} as const)[mode];return this.perform(()=>this.dl.setDl24mTarget({...p,dl24mTargetRequest:{mode,value,unit}}));}
 async enable(){if(!this.canOn||!this.supports('output'))throw Error('ON requires a supported mode, fresh state and confirmed OFF');const p=this.params();return this.perform(()=>this.components.setComponentOutput({...p,outputRequest:{enabled:true}}));}
 async stop(){
  this.requests.cancelPending(Error('Cancelled by priority STOP'),job=>job.priority===2);
  if(!this.device)throw Error('Select a DL24M device first');
  const deviceId=this.device.id;
  if(this.stream.ready)return this.perform(()=>this.core.stopDevice({deviceId,idempotencyKey:key(),stopRequest:{reason:'DL24M Bench STOP'}}),true);
  // Explicit emergency recovery is independent of WebSocket ownership/freshness.
  // It never retries a write and does not re-establish control or a reservation.
  const generation=++this.operationGeneration;this.busy=true;this.onChange();
  const config=new Configuration({basePath:location.origin,accessToken:()=>this.token,fetchApi:async(input,init)=>{
   const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),4000);
   try{return await fetch(input,{...init,signal:controller.signal});}finally{clearTimeout(timer);}
  }}),core=new CoreApi(config),components=new ComponentsApi(config);
  try{
   let operation=await core.stopDevice({deviceId,idempotencyKey:key(),stopRequest:{reason:'DL24M Bench explicit HTTP STOP recovery'}});
   const deadline=performance.now()+12000;
   while(operation.status==='running'||operation.status==='pending'){
    if(performance.now()>deadline)throw Error('STOP result uncertain; OFF remains unconfirmed');
    await new Promise(r=>setTimeout(r,150));operation=await core.getOperation({operationId:operation.id});
   }
   if(operation.status!=='succeeded'||operation.finalOutput!=='off'||operation.restorePending)throw Error('STOP did not confirm OFF and completed recovery');
   const actor=await components.getActuatorState({deviceId,componentId:this.sink!});
   if(actor.output!=='off')throw Error('OFF readback was not confirmed');
   this.actor=actor;this.uncertain=false;return operation;
  }catch(error){this.uncertain=true;throw error;}
  finally{if(generation===this.operationGeneration){this.busy=false;this.onChange();}}
 }

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
  this.invalidate();this.stopHeartbeat();
  if(this.lease&&this.device)void this.core.stopDevice({deviceId:this.device.id,idempotencyKey:key(),stopRequest:{reason:'Bench page closed'}},{keepalive:true}).catch(()=>{});
 }
}
