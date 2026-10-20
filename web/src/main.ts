import {Configuration,CoreApi,ResponseError} from '../generated/src/index';
import type {Device,Operation} from '../generated/src/index';
import {renderDevicePanels} from './modules/index';
import {renderSetupPanels} from './modules/setup';
import {UiAuth} from './auth';
const app=document.querySelector<HTMLElement>('#app')!, status=document.querySelector<HTMLElement>('#status')!;
let lease='',heartbeat:number|undefined;
const auth=new UiAuth();
const config=new Configuration({basePath:location.origin,fetchApi:auth.fetch});
const core=new CoreApi(config);
auth.core=core;
auth.report=report;
auth.notify=message=>{status.className='';status.textContent=message;};
auth.onLost=()=>{lease='';clearInterval(heartbeat);app.replaceChildren();auth.render(app,currentPage);};
auth.beforeLogout=async()=>{
 const current=lease;clearInterval(heartbeat);lease='';
 if(current){await core.stopLease({leaseId:current,stopRequest:{reason:'UI sign out'}});status.textContent='Shutdown requested; verify outputs OFF.';}
};
function el<K extends keyof HTMLElementTagNameMap>(tag:K,text='',parent:HTMLElement=app){const e=document.createElement(tag);e.textContent=text;parent.append(e);return e;}
function report(error:unknown){void (async()=>{
 let message=error instanceof Error?error.message:String(error);
 if(error instanceof ResponseError){try{const body=await error.response.clone().json();message=body.detail||message;}catch{/* Keep the transport error. */}
  if(error.response.status===409)message=`Devices or network are busy, or the request conflicts with current state. ${message}`;
  if(error.response.status===429)message=`Sign-in temporarily limited. ${message}`;
 }
 status.className='error';status.textContent=message;
})();}
function action(parent:HTMLElement,label:string,fn:()=>Promise<unknown>,disabled=false){const b=el('button',label,parent);b.disabled=disabled;b.onclick=async()=>{b.disabled=true;try{await fn();status.className='';}catch(e){report(e);}finally{b.disabled=disabled;}};return b;}
function input(parent:HTMLElement,label:string,type='text'){const l=el('label',label,parent),i=el('input','',l);i.type=type;return i;}
function key(){const bytes=new Uint8Array(16);crypto.getRandomValues(bytes);return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');}
async function wait(o:Operation){const until=Date.now()+12000;while(o.status==='running'||o.status==='pending'){if(Date.now()>until)throw Error(`Operation ${o.id} is uncertain; query it before repeating`);await new Promise(r=>setTimeout(r,100));o=await core.getOperation({operationId:o.id});}status.textContent=JSON.stringify(o,null,2);await instruments();if(o.status!=='succeeded')throw Error(`${o.status}: ${o.stage}; output ${o.finalOutput}`);return o;}
function params(d:Device){if(!lease)throw Error('Reserve instruments first');return {deviceId:d.id,xLabLease:lease,idempotencyKey:key()};}
async function wifi(){
 app.replaceChildren();auth.render(app,currentPage);
 const s=el('section');el('h2','Wi-Fi setup',s);el('p','Choose the target network below. Setup-AP Wi-Fi configuration works without signing in; access through your LAN requires the device password.',s);
 const selection=el('select','',s),ssid=input(s,'Network name (SSID)'),psk=input(s,'Wi-Fi password (PSK)','password');psk.autocomplete='new-password';
 selection.onchange=()=>{ssid.value=selection.value;};
 async function scan(){status.textContent='Scanning Wi-Fi…';let found=await core.scanWifi();const deadline=Date.now()+15000;while(found.state==='scanning'){if(!s.isConnected)return;if(Date.now()>deadline)throw Error('Scan timeout');await new Promise(r=>setTimeout(r,300));found=await core.scanWifi();}if(!s.isConnected)return;if(found.state==='failed')throw Error('Wi-Fi scan failed; try again');selection.replaceChildren();for(const net of found.networks){const o=el('option',`${net.ssid} (${net.rssiDbm} dBm)`,selection);o.value=net.ssid;}if(found.networks.length)ssid.value=found.networks[0].ssid;status.textContent=`${found.networks.length} networks found`;}
 action(s,'Scan again',scan);action(s,'Save and connect',async()=>{await core.configureWifi({wifiRequest:{ssid:ssid.value,psk:psk.value}});psk.value='';status.textContent='Network saved. Reconnect to the configured network if needed.';});
 try{await scan();}catch(e){if(s.isConnected)report(e);}
}
async function instruments(){
 app.replaceChildren();const controls=el('section');el('h2','Instruments',controls);auth.render(app,currentPage);
 if(!auth.state.authenticated)return;
 action(controls,'Refresh',instruments);
 action(controls,lease?'Release and stop':'Reserve instruments',async()=>{
  if(lease){await core.stopLease({leaseId:lease,stopRequest:{reason:'UI release'}});lease='';clearInterval(heartbeat);}
  else{const devices=(await core.listDevices()).devices;const l=await core.createLease({leaseRequest:{deviceIds:devices.map(d=>d.id),ttlMs:10000}});lease=l.id;
   heartbeat=window.setInterval(()=>{core.heartbeatLease({leaseId:lease,stopRequest:{}}).catch(e=>{lease='';clearInterval(heartbeat);report(e);void instruments();});},1000);}
  await instruments();
 });
 const devices=(await core.listDevices()).devices;
 for(const d of devices){const panel=el('section');el('h3',`${d.id} · ${d.driver}`,panel);el('p',`${d.state} · output ${d.output} · ${d.identity}`,panel);
  const reading=el('pre','',panel);action(panel,'Read measurements',async()=>{reading.textContent=JSON.stringify(await core.getMeasurements({deviceId:d.id}),null,2);});
  renderDevicePanels({core,config,reading,leased:!!lease,params,input:(label,type)=>input(panel,label,type),action:(label,fn,disabled)=>action(panel,label,fn,disabled),wait,key},d);
 }
 renderSetupPanels({core,config,parent:app,leased:!!lease,report,notify:message=>{status.textContent=message;}});
}
async function currentPage(){await (location.pathname==='/wifi'||location.pathname==='/'?wifi():instruments());}
void auth.refresh().then(currentPage).catch(report);
