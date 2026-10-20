import {mockAuth,signIn} from './auth-helper.js';
import {test,expect} from '@playwright/test';
const device=(id,driver,capabilities)=>({id,driver,identity:'synthetic-'+id,transport:'test',capabilities,state:'ready',output:'off',limits:{voltage_v:25,current_a:.2,power_w:5,max_age_ms:3000}});
async function mock(page,devices=[device('load-2','dl24m',['output','load.cc','dl24m.settings'])]){
 const writes=[],operations=new Map();let scans=0;
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname,method=r.method(),body=r.postDataJSON();
  const fulfill=(value,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(value)});
  if(path==='/api/v1/wifi/scan')return fulfill({state:scans++?'complete':'scanning',networks:scans>1?[{ssid:'Lab <b>AP</b>',rssi_dbm:-40,secured:true}]:[]});
  if(path==='/api/v1/wifi'){writes.push({path,method,body,headers:r.headers()});return fulfill({state:'connecting',hostname:'test.local'});}
  if(!r.headers().cookie?.includes('LabBridgeSession=synthetic-session'))return fulfill({type:'about:blank',title:'Unauthorized',status:401,detail:'Token required'},401);
  if(method==='GET'&&path==='/api/v1/devices')return fulfill({devices});
  if(path==='/api/v1/leases'&&method==='POST')return fulfill({id:'lease-1',device_ids:body.device_ids,expires_at_ms:100000,state:'active'});
  if(path.startsWith('/api/v1/leases/'))return fulfill({id:'lease-1',device_ids:devices.map(d=>d.id),expires_at_ms:100000,state:path.endsWith('/stop')?'stopped':'active'});
  if(path.startsWith('/api/v1/operations/')){
   const item=operations.get(path.split('/').at(-1));if(++item.polls>=2)item.operation.status='succeeded';return fulfill(item.operation);
  }
  if(path.endsWith('/temperature/channels'))return fulfill({channels:[{id:'cell',kind:'cell',quality:'valid',value_c:25,received_ms:0,age_ms:0}]});
  if(method==='GET'&&path.includes('/components/')&&path.endsWith('/state')){const d=devices.find(d=>path.includes('/'+d.id+'/'));return fulfill({device_id:d.id,component_id:path.split('/').at(-2),state:d.state,output:d.output,settings:{}});}
  if(method==='GET'&&path.endsWith('/readings'))return fulfill({device_id:path.split('/')[4],boot_id:'test-boot',readings:[{device_id:path.split('/')[4],component_id:'current',boot_id:'test-boot',sequence:1,received_ms:0,age_ms:0,quality:'valid',quantity:'current',unit:'A',value:-.1}]});
  if(method==='GET'&&path.endsWith('/measurements'))return fulfill({device_id:path.split('/')[4],sequence:1,received_ms:0,age_ms:0,quality:'valid',temperature_channels:[],voltage_v:24,current_a:0,power_w:0});
  if(method==='GET'&&path.endsWith('/dl24m/settings'))return fulfill({mode:0,cutoff_voltage_v:.7,time_limit_s:0});
  if(method==='PUT'||method==='POST'){
   writes.push({path,method,body,headers:r.headers()});const d=devices.find(d=>path.includes('/'+d.id+'/'));
   if(path.endsWith('/stop')){d.output='off';d.state='ready';}
   if(path.endsWith('/output'))d.output=body.enabled?'on':'off';
   const operation={id:'op-'+(operations.size+1),device_id:d.id,status:'running',stage:'readback',read_attempts:1,write_attempts:1,reconnects:0,duration_ms:50,final_output:d.output,restore_pending:false};operations.set(operation.id,{operation,polls:0});return fulfill(operation,202);
  }
  return fulfill({type:'about:blank',title:'Not found',status:404,detail:'Unmocked route'},404);
 });
 await mockAuth(page);
 return writes;
}
async function authenticate(page){await signIn(page);}
test('Wi-Fi scan, literal SSID selection and PSK save are separate from instruments',async({page})=>{
 const writes=await mock(page);await page.goto('/');await expect(page.getByRole('heading',{name:'Wi-Fi setup'})).toBeVisible();
 await expect(page.getByLabel('Network name (SSID)')).toHaveValue('Lab <b>AP</b>');await expect(page.locator('b')).toHaveCount(0);
 await page.getByLabel('Wi-Fi password (PSK)').fill('test-password');await page.getByRole('button',{name:'Save and connect'}).click();
 await expect(page.getByLabel('Wi-Fi password (PSK)')).toHaveValue('');expect(writes[0].body).toEqual({ssid:'Lab <b>AP</b>',psk:'test-password'});
 await page.getByRole('link',{name:'Instruments',exact:true}).click();await expect(page.getByRole('heading',{name:'Instruments',exact:true})).toBeVisible();await expect(page.getByRole('heading',{name:'Wi-Fi setup'})).toHaveCount(0);
});
test('load module uses selected instance, lease, idempotency and asynchronous result',async({page})=>{
 const writes=await mock(page);await page.goto('/devices');await authenticate(page);await expect(page.getByRole('heading',{name:'load-2 · dl24m'})).toBeVisible();
 await expect(page.getByRole('button',{name:'Output ON'})).toBeDisabled();await page.getByRole('button',{name:'Reserve instruments'}).click();await expect(page.getByRole('button',{name:'Release and stop'})).toBeVisible();
 await page.getByLabel('Load current (A)').fill('.15');await page.getByRole('button',{name:'Set load current'}).click();await expect(page.locator('#status')).toContainText('succeeded');
 const write=writes.find(w=>w.path.endsWith('/components/sink/current'));expect(write.path).toBe('/api/v1/devices/load-2/components/sink/current');expect(write.body).toEqual({current_a:.15});expect(write.headers['x-lab-lease']).toBe('lease-1');expect(write.headers['idempotency-key']).toHaveLength(32);
 await page.getByRole('button',{name:'Output ON'}).click();await expect(page.locator('#status')).toContainText('"finalOutput": "on"');await expect(page.getByRole('button',{name:'Output ON'})).toBeDisabled();
});
test('an expired session shows login and never replays the failed actuator write',async({page})=>{
 await mock(page);await page.goto('/devices');await authenticate(page);
 await page.getByRole('button',{name:'Reserve instruments',exact:true}).click();
 await expect(page.getByRole('button',{name:'Release and stop',exact:true})).toBeVisible();
 let attempts=0;
 await page.route('**/api/v1/devices/load-2/components/sink/current',route=>{
  ++attempts;return route.fulfill({status:401,contentType:'application/problem+json',body:JSON.stringify({detail:'Session expired'})});
 });
 await page.getByLabel('Load current (A)').fill('.15');await page.getByRole('button',{name:'Set load current',exact:true}).click();
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 await authenticate(page);await expect(page.getByRole('heading',{name:'load-2 · dl24m'})).toBeVisible();
 expect(attempts).toBe(1);
});
test('sign out attempts lease STOP before revoking the session, including failed STOP',async({page})=>{
 await mock(page);await page.goto('/devices');await authenticate(page);
 await page.getByRole('button',{name:'Reserve instruments',exact:true}).click();
 await expect(page.getByRole('button',{name:'Release and stop',exact:true})).toBeVisible();
 const calls=[];page.on('request',request=>{if(request.url().endsWith('/stop')||request.url().endsWith('/auth/session'))calls.push(request);});
 await page.route('**/api/v1/leases/lease-1/stop',route=>route.fulfill({status:503,contentType:'application/problem+json',body:JSON.stringify({detail:'Unavailable'})}));
 await page.getByRole('button',{name:'Sign out',exact:true}).click();
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 await expect(page.locator('#status')).toContainText('could not be confirmed');
 expect(calls.map(r=>new URL(r.url()).pathname)).toEqual(['/api/v1/leases/lease-1/stop','/api/v1/auth/session']);
 expect(calls[0].headers()['x-lab-csrf']).toBe('synthetic-csrf');
});
test('unknown output retains an independent STOP without a lease',async({page})=>{
 const d=device('unavailable-load','dl24m',['output','load.cc']);d.state='fault';d.output='unknown';const writes=await mock(page,[d]);await page.goto('/devices');await authenticate(page);
 await expect(page.getByRole('button',{name:'Output ON'})).toBeDisabled();await page.getByRole('button',{name:'STOP',exact:true}).click();await expect(page.locator('#status')).toContainText('succeeded');
 expect(writes[0].path).toBe('/api/v1/devices/unavailable-load/stop');expect(writes[0].headers['x-lab-lease']).toBeUndefined();
});
test('mixed module panels render without core device-name branches',async({page})=>{
 await mock(page,[device('psu-a','rd60xx',['output','source.cc_cv']),device('paths','gpio_interlock',['output','switch.path','switch.feedback']),device('probe','ds18b20',['temperature.external'])]);await page.goto('/devices');await authenticate(page);
 await expect(page.getByLabel('Source voltage (V)')).toBeVisible();await expect(page.getByRole('button',{name:'Path: isolated'})).toBeEnabled();await page.getByRole('button',{name:'Read temperature channels'}).click();await expect(page.getByText('"unit": "A"',{exact:false})).toBeVisible();
});
test('a composite device and an independent meter use common component routes',async({page})=>{
 const combo=device('combined','unfamiliar-source',['output','components']);
 combo.components=[{id:'channel-a',kind:'source',actions:['current','voltage','output']},{id:'channel-b',kind:'source',actions:['current','voltage','output']}];
 const meter=device('inline','ina228',['components']);delete meter.limits;
 meter.components=[{id:'current',kind:'sensor',actions:[],quantity:'current',unit:'A'},{id:'temperature-die',kind:'sensor',actions:[],quantity:'temperature',unit:'degC',temperature_kind:'internal'}];
 const writes=await mock(page,[combo,meter]);await page.goto('/devices');await authenticate(page);
 await expect(page.getByLabel('channel-b current (A)')).toBeVisible();
 await page.getByRole('button',{name:'Reserve instruments'}).click();await expect(page.getByRole('button',{name:'Release and stop'})).toBeVisible();
 await page.getByRole('button',{name:'channel-b: set current'}).click();await expect(page.locator('#status')).toContainText('valid numeric value');expect(writes).toHaveLength(0);
 await page.getByLabel('channel-b current (A)').fill('.12');await page.getByRole('button',{name:'channel-b: set current'}).click();
 await expect(page.locator('#status')).toContainText('succeeded');
 expect(writes.find(w=>w.path.endsWith('/components/channel-b/current')).body).toEqual({current_a:.12});
 await page.getByRole('button',{name:'channel-b: ON',exact:true}).click();await expect(page.locator('#status')).toContainText('"finalOutput": "on"');
 await expect(page.getByRole('button',{name:'channel-b: ON',exact:true})).toBeDisabled();
 await page.getByRole('button',{name:'channel-b: STOP',exact:true}).click();await expect(page.locator('#status')).toContainText('"finalOutput": "off"');
 expect(writes.find(w=>w.path.endsWith('/components/channel-b/stop')).headers['x-lab-lease']).toBeUndefined();
 await page.getByRole('button',{name:'Read sensors: inline'}).click();await expect(page.getByText('"unit": "A"',{exact:false})).toBeVisible();
});
