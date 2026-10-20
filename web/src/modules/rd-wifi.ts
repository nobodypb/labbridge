import {Rd60xxApi} from '../../generated/src/index';
import type {Configuration,CoreApi,RdProvisioning} from '../../generated/src/index';
export interface SetupContext {config:Configuration;core:CoreApi;parent:HTMLElement;leased:boolean;report:(error:unknown)=>void;notify:(message:string)=>void;}
export const rdWifiSetup={id:'rd-wifi',render(c:SetupContext){
 const root=document.createElement('details');c.parent.append(root);
 function el<K extends keyof HTMLElementTagNameMap>(tag:K,text='',parent:HTMLElement=root){const e=document.createElement(tag);e.textContent=text;parent.append(e);return e;}
 function input(label:string,type='text'){const l=el('label',label),i=el('input','',l);i.type=type;return i;}
 function action(label:string,fn:()=>Promise<unknown>){const b=el('button',label);b.onclick=async()=>{b.disabled=true;try{await fn();}catch(e){c.report(e);}finally{update();}};return b;}
 el('summary','RD60xx Wi-Fi setup');
 el('p','The open ESP AP hosts the PSU connection at 192.168.4.1:8080 (default SSID LabBridge-Setup). Stop outputs and release leases first. On the target PSU select WiFi and Modbus address 1, reset its previous WiFi setup, and restart with output OFF. Configure one PSU at a time.');
 const confirmed=input('Target PSU output is OFF','checkbox');
 const state=el('pre');state.dataset.testid='rd-pair-state';
 const api=new Rd60xxApi(c.config);let value:RdProvisioning|undefined,poll:number|undefined;
 const active=()=>['server_address','wifi','waiting_connection'].includes(value?.state??'');
 async function show(v:RdProvisioning){
  value=v;state.textContent=JSON.stringify(v,null,2);selection.replaceChildren();
  for(const d of v.candidates.filter(d=>d.connected&&!d.configured)){
   const option=el('option',`${d.model} · ${d.serial}`,selection);option.value=d.serial;
  }
  update();window.clearTimeout(poll);
  if(active())poll=window.setTimeout(async()=>{if(!root.isConnected)return;try{await show(await api.getRdProvisioning());}catch(e){c.report(e);}},500);
 }
 const refresh=action('Read RD connections',async()=>show(await api.getRdProvisioning()));
 const start=action('Start RD pairing',async()=>{const result=await api.startRdProvisioning({rdProvisionStart:{confirmOutputsOff:true}});confirmed.checked=false;await show(result);});
 const server=action('ESP address shown / Connecting WiFi',async()=>show(await api.advanceRdProvisioning({rdProvisionAdvance:{sessionId:value!.sessionId!,expectedPhase:'server_address'}})));
 const wifi=action('Connecting server shown',async()=>show(await api.advanceRdProvisioning({rdProvisionAdvance:{sessionId:value!.sessionId!,expectedPhase:'wifi'}})));
 const cancel=action('Cancel RD pairing',async()=>show(await api.cancelRdProvisioning({rdProvisionCancel:{sessionId:value!.sessionId!}})));
 const selectionLabel=el('label','Discovered RD source'),selection=el('select','',selectionLabel);
 el('p','Discovery is read-only. To register a source, enter explicit limits appropriate for the connected circuit. Saving restarts the bridge. Leave the PSU output OFF; a PSU restart may be needed to reconnect.');
 const id=input('RD device ID'),voltage=input('RD voltage limit (V)','number'),current=input('RD current limit (A)','number'),power=input('RD power limit (W)','number');
 for(const field of [voltage,current,power])field.step='any';
 function positive(field:HTMLInputElement,max:number){const v=field.valueAsNumber;if(!field.value.trim()||!Number.isFinite(v)||v<=0||v>max)throw Error('Enter positive RD limits within the model rating');return v;}
 const add=action('Add RD source and restart',async()=>{
  const candidate=value?.candidates.find(d=>d.serial===selection.value&&d.connected&&!d.configured);
  if(!candidate)throw Error('Select a connected, unconfigured RD source');
  const a=positive(current,candidate.model==='RD6018'?18:24),v=positive(voltage,60),w=positive(power,candidate.model==='RD6018'?1080:1440);
  if(!/^[A-Za-z0-9_-]{1,48}$/.test(id.value))throw Error('Enter a valid RD device ID');
  const raw=await c.core.getConfigurationRaw(),configuration=await raw.value(),etag=raw.raw.headers.get('ETag');
  if(!etag)throw Error('Configuration ETag missing');
  if(configuration.devices.some(d=>d.id===id.value||(d.driver==='rd60xx'&&d.endpoint===candidate.serial)))throw Error('Device ID or physical serial is already configured');
  configuration.devices.push({id:id.value,driver:'rd60xx',transport:'tcp',endpoint:candidate.serial,pins:[],rd60xx:{model:candidate.model==='RD6018'?6018:6024},limits:{voltageV:v,currentA:a,powerW:w,maxAgeMs:3000}});
  await c.core.setConfiguration({ifMatch:etag,nodeConfiguration:configuration});c.notify('RD source saved. Bridge restarts; reload after reconnection.');
 });
 function update(){start.disabled=c.leased||!confirmed.checked||active();server.disabled=c.leased||value?.state!=='server_address';wifi.disabled=c.leased||value?.state!=='wifi';cancel.disabled=!active();add.disabled=c.leased||active()||!value?.candidates.some(d=>d.connected&&!d.configured);refresh.disabled=false;}
 confirmed.onchange=update;update();
}};
