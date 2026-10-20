import {Configuration,CoreApi,Dl24mApi,Rd60xxApi,GpioInterlockApi,Ds18b20Api} from '../generated/src/index';
import type {Device,Operation} from '../generated/src/index';
import {panelIds,canEnable} from './panels.js';
const app=document.querySelector<HTMLElement>('#app')!, status=document.querySelector<HTMLElement>('#status')!;
let token='',lease='',heartbeat:number|undefined;
const config=new Configuration({basePath:location.origin,accessToken:()=>token});
const core=new CoreApi(config),load=new Dl24mApi(config),source=new Rd60xxApi(config),sw=new GpioInterlockApi(config),temperature=new Ds18b20Api(config);
function el<K extends keyof HTMLElementTagNameMap>(tag:K,text='',parent:HTMLElement=app){const e=document.createElement(tag);e.textContent=text;parent.append(e);return e;}
function report(error:unknown){status.className='error';status.textContent=error instanceof Error?error.message:String(error);}
function action(parent:HTMLElement,label:string,fn:()=>Promise<unknown>,disabled=false){const b=el('button',label,parent);b.disabled=disabled;b.onclick=async()=>{b.disabled=true;try{await fn();status.className='';}catch(e){report(e);}finally{b.disabled=disabled;}};return b;}
function input(parent:HTMLElement,label:string,type='text'){const l=el('label',label,parent),i=el('input','',l);i.type=type;return i;}
function key(){return crypto.randomUUID();}
async function wait(o:Operation){const until=Date.now()+12000;while(o.status==='running'||o.status==='pending'){if(Date.now()>until)throw Error(`Operation ${o.id} is uncertain; query it before repeating`);await new Promise(r=>setTimeout(r,100));o=await core.getOperation({operationId:o.id});}status.textContent=JSON.stringify(o,null,2);if(o.status!=='succeeded')throw Error(`${o.status}: ${o.stage}; output ${o.finalOutput}`);return o;}
function params(d:Device){if(!lease)throw Error('Reserve instruments first');return {deviceId:d.id,xLabLease:lease,idempotencyKey:key()};}
async function wifi(){
 const s=el('section');el('h2','Wi-Fi setup',s);el('p','Connect to the open LabBridge-Setup access point. Enter the password of your target network below.',s);
 const selection=el('select','',s),ssid=input(s,'Network name (SSID)'),psk=input(s,'Wi-Fi password (PSK)','password');psk.autocomplete='new-password';
 selection.onchange=()=>{ssid.value=selection.value;};
 async function scan(){const found=await core.scanWifi();selection.replaceChildren();for(const net of found.networks){const o=el('option',`${net.ssid} (${net.rssiDbm} dBm)`,selection);o.value=net.ssid;}if(found.networks.length)ssid.value=found.networks[0].ssid;status.textContent=`${found.networks.length} networks found`;}
 action(s,'Scan again',scan);action(s,'Save and connect',async()=>{await core.configureWifi({wifiRequest:{ssid:ssid.value,psk:psk.value}});psk.value='';status.textContent='Network saved. Reconnect to the configured network if needed.';});
 try{await scan();}catch(e){report(e);}
}
async function instruments(){
 app.replaceChildren();const auth=el('section');el('h2','Instruments',auth);const password=input(auth,'API bearer token','password');password.value=token;password.onchange=()=>{token=password.value;};
 action(auth,'Refresh',async()=>{token=password.value;await instruments();});
 action(auth,lease?'Release and stop':'Reserve instruments',async()=>{
  if(lease){await core.stopLease({leaseId:lease,stopRequest:{reason:'UI release'}});lease='';clearInterval(heartbeat);}
  else{const devices=(await core.listDevices()).devices;const l=await core.createLease({leaseRequest:{deviceIds:devices.map(d=>d.id),ttlMs:10000}});lease=l.id;
   heartbeat=window.setInterval(()=>{core.heartbeatLease({leaseId:lease,stopRequest:{}}).catch(e=>{lease='';clearInterval(heartbeat);report(e);void instruments();});},1000);}
  await instruments();
 });
 const devices=(await core.listDevices()).devices;
 for(const d of devices){const panel=el('section');el('h3',`${d.id} · ${d.driver}`,panel);el('p',`${d.state} · output ${d.output} · ${d.identity}`,panel);
  const reading=el('pre','',panel);action(panel,'Read measurements',async()=>{reading.textContent=JSON.stringify(await core.getMeasurements({deviceId:d.id}),null,2);});
  for(const module of panelIds(d)){
   if(module==='output'){
    action(panel,'Output ON',async()=>wait(await core.setOutput({...params(d),outputRequest:{enabled:true}})),!canEnable(d,!!lease));
    action(panel,'STOP',async()=>wait(await core.stopDevice({deviceId:d.id,idempotencyKey:key(),stopRequest:{reason:'UI stop'}})));
   }else if(module==='load'){
    const a=input(panel,'Load current (A)','number');a.min='0';a.max=String(d.limits.currentA);a.step='.001';
    action(panel,'Set load current',async()=>wait(await load.setLoadCurrent({...params(d),currentRequest:{currentA:Number(a.value)}})),!lease);
   }else if(module==='source'){
    const v=input(panel,'Source voltage (V)','number'),a=input(panel,'Source current (A)','number');v.min=a.min='0';v.max=String(d.limits.voltageV);a.max=String(d.limits.currentA);v.step=a.step='.01';
    action(panel,'Set voltage',async()=>wait(await source.setSourceVoltage({...params(d),voltageRequest:{voltageV:Number(v.value)}})),!lease);
    action(panel,'Set current',async()=>wait(await source.setSourceCurrent({...params(d),currentRequest:{currentA:Number(a.value)}})),!lease);
   }else if(module==='interlock'){
    for(const path of ['isolated','charge','discharge'] as const)action(panel,`Path: ${path}`,async()=>wait(await sw.setSwitchPath({...params(d),switchPathRequest:{path}})),path!=='isolated'&&!lease);
   }else if(module==='temperature')action(panel,'Read temperature channels',async()=>{reading.textContent=JSON.stringify(await temperature.getTemperatureChannels({deviceId:d.id}),null,2);});
   else if(module==='settings'){
    action(panel,'Read load settings',async()=>{reading.textContent=JSON.stringify(await load.getDl24mSettings({deviceId:d.id}),null,2);});
    const cutoff=input(panel,'Discharge cutoff (V)','number');cutoff.min='0';cutoff.step='.1';
    action(panel,'Set cutoff',async()=>wait(await load.setDl24mSettings({...params(d),dl24mSettings:{cutoffVoltageV:Number(cutoff.value)}})),!lease);
    action(panel,'Reset counters',async()=>wait(await load.resetDl24mCounters({...params(d),stopRequest:{reason:'UI counter reset'}})),!lease);
   }
  }
 }
}
void (location.pathname==='/wifi'||location.pathname==='/'?wifi():instruments()).catch(report);
