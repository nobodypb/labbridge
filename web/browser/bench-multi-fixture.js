import {mockAuth,signIn} from './auth-helper.js';
import {expect} from '@playwright/test';
export async function mixedBackend(page,{onlyMeters=false,waveform=false}={}){
 const sensors=[['voltage','V'],['current','A'],['power','W']].map(([id,unit])=>({id,kind:'sensor',quantity:id,unit,actions:[]}));
 const make=(id,driver,component,actions,extra=[])=>({id,driver,identity:`synthetic-${id}`,transport:'tcp',state:'ready',output:'off',capabilities:driver==='dl24m'?['dl24m.settings']:[],limits:{voltage_v:60,current_a:2,power_w:100,max_age_ms:3000},components:[...(component?[{id:component,kind:driver==='dl24m'?'sink':'source',actions}]:[]),...sensors,...extra]});
 const devices=onlyMeters?[make('meter','ina228',null,[])]:[
  make('load','dl24m','sink',['current','output','cutoff','duration']),
  make('supply','rd60xx','source',['voltage','current','output'],[['input-voltage','voltage','V'],['battery-voltage','voltage','V'],['battery-detected','presence','1'],['charge','charge','Ah'],['energy','energy','Wh'],['temperature-external','temperature','degC']].map(([id,quantity,unit])=>({id,kind:'sensor',quantity,unit,actions:[],...(id==='temperature-external'?{temperature_kind:'external_probe'}:{})}))),
  make('supply-b','rd60xx','source',['voltage','current','output']),
  make('future','synthetic-driver','arbitrary-actor',['voltage','current','output']),
  make('meter','yocto_watt',null,[])
 ];
 const state={devices,calls:[],writes:[],outputs:Object.fromEntries(devices.map(d=>[d.id,'off'])),settings:{mode:0,working_brightness:5},leases:new Map(),operations:new Map(),hold:new Set(),failStops:new Set(),stale:new Set(),unavailable:new Set(),epoch:'1',boot:'boot-a',failHeartbeat:new Set(),measurements:new Map(),next:0};
 let auth;
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname,method=r.method(),body=r.postDataJSON(),id=path.split('/')[4],device=devices.find(d=>d.id===id);
  const reply=(data,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)});
  state.calls.push({path,method});
  if(!r.headers().cookie?.includes('LabBridgeSession=synthetic-session')||auth.revoked)return reply({detail:'Sign in required'},401);
  if(method!=='GET'){
   if(r.headers()['x-lab-csrf']!=='synthetic-csrf')return reply({detail:'CSRF required'},403);
   state.writes.push({path,body,headers:r.headers()});
  }
  if(path==='/api/v1/node')return reply({node_id:'synthetic-node',boot_id:state.boot,hostname:'synthetic.local',board:'test',firmware_version:'test',api_version:'1'});
  if(path==='/api/v1/devices')return reply({devices:devices.map(d=>({...d,output:state.outputs[d.id]}))});
  if(path==='/api/v1/leases'){
   const lease={id:`lease-${body.device_ids[0]}`,device_ids:body.device_ids,state:'active',expires_at_ms:999999};state.leases.set(lease.id,lease);return reply(lease);
  }
  if(path.endsWith('/heartbeat'))return state.failHeartbeat.has(id)?reply({detail:'Lease lost'},423):reply(state.leases.get(id));
  if(path.startsWith('/api/v1/leases/')&&path.endsWith('/stop'))return reply({...state.leases.get(id),state:'stopped'});
  const t=Math.floor(Date.now()/100)*100,quality=state.stale.has(id)?'stale':state.unavailable.has(id)?'unavailable':'valid';
  const phase=t/1600+devices.findIndex(d=>d.id===id),v=12+Math.sin(phase)*.7,i=.2+Math.cos(phase*1.3)*.03,w=v*i;
  if(path.endsWith('/measurements'))return reply({device_id:id,boot_id:state.boot,sequence:t,received_ms:t,age_ms:0,quality,voltage_v:waveform?v:12,current_a:waveform?i:.2,power_w:waveform?w:2.4,temperature_channels:[],set_voltage_v:12,set_current_a:.2,regulation_mode:'cv',protection_code:0,input_voltage_v:24,battery_voltage_v:0,battery_detected:false,capacity_ah:5,energy_wh:60,...state.measurements.get(id)});
  if(path.endsWith('/readings'))return reply({device_id:id,boot_id:state.boot,readings:device.components.filter(c=>c.kind==='sensor').map(c=>({device_id:id,component_id:c.id,boot_id:state.boot,epoch:state.epoch,quantity:c.quantity,unit:c.unit,quality:c.id==='temperature-external'?'unavailable':quality,sequence:t,received_ms:t,age_ms:0,...(c.id==='temperature-external'?{}:{value:({'V':waveform?v:12,'A':waveform?i:.2,'W':waveform?w:2.4,'Ah':5,'Wh':60,'1':0})[c.unit]})}))});
  if(path.endsWith('/state'))return reply({device_id:id,component_id:path.split('/')[6],state:'ready',output:state.outputs[id],settings:{current_a:.2,voltage_v:12,regulation_mode:'cv'}});
  if(path.endsWith('/dl24m/settings')&&method==='GET')return device.driver==='dl24m'?reply(state.settings):reply({detail:'Wrong device API'},404);
  if(path.includes('/operations/')){
   const op=state.operations.get(id);if(!state.hold.has(op.device_id)){
    op.status=op.stop&&state.failStops.has(op.device_id)?'failed':'succeeded';
    if(op.status==='succeeded')state.outputs[op.device_id]=op.final_output;
   }return reply(op);
  }
  if(method!=='GET'&&device){
   const stop=path.endsWith('/stop'),op={id:`op-${++state.next}`,device_id:id,status:'running',stage:'readback',read_attempts:1,write_attempts:1,reconnects:0,duration_ms:10,restore_pending:false,final_output:stop?'off':path.endsWith('/output')?(body.enabled?'on':'off'):state.outputs[id],stop};
   if(path.endsWith('/dl24m/settings'))Object.assign(state.settings,body);
   state.operations.set(op.id,op);return reply(op,202);
  }
  return reply({detail:'Unmocked '+path},404);
 });
 auth=await mockAuth(page);state.auth=auth;return state;
}
export async function openBench(page){await page.goto('/dl24');await signIn(page);await expect(page.locator('#connLbl')).toHaveText('Connected');}
export async function selectReserve(page,id){await page.locator('#deviceSelect').selectOption(id);await page.locator('#reserve').click();await expect(page.locator('#reserve')).toHaveText('Release and stop');}
