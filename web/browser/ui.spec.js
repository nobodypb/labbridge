import {test,expect} from '@playwright/test';
const device=(id,driver,capabilities)=>({id,driver,identity:'synthetic-'+id,transport:'test',capabilities,state:'ready',output:'off',limits:{voltage_v:25,current_a:.2,power_w:5,max_age_ms:3000}});
async function mock(page,devices=[device('load-2','dl24m',['output','load.cc','dl24m.settings'])]){
 const writes=[],operations=new Map();let scans=0;
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname,method=r.method(),body=r.postDataJSON();
  const fulfill=(value,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(value)});
  if(path==='/api/v1/wifi/scan')return fulfill({state:scans++?'complete':'scanning',networks:scans>1?[{ssid:'Lab <b>AP</b>',rssi_dbm:-40,secured:true}]:[]});
  if(path==='/api/v1/wifi'){writes.push({path,method,body,headers:r.headers()});return fulfill({state:'connecting',hostname:'test.local'});}
  if(r.headers().authorization!=='Bearer test-token')return fulfill({type:'about:blank',title:'Unauthorized',status:401,detail:'Token required'},401);
  if(method==='GET'&&path==='/api/v1/devices')return fulfill({devices});
  if(path==='/api/v1/leases'&&method==='POST')return fulfill({id:'lease-1',device_ids:body.device_ids,expires_at_ms:100000,state:'active'});
  if(path.startsWith('/api/v1/leases/'))return fulfill({id:'lease-1',device_ids:devices.map(d=>d.id),expires_at_ms:100000,state:path.endsWith('/stop')?'stopped':'active'});
  if(path.startsWith('/api/v1/operations/')){
   const item=operations.get(path.split('/').at(-1));if(++item.polls>=2)item.operation.status='succeeded';return fulfill(item.operation);
  }
  if(path.endsWith('/temperature/channels'))return fulfill({channels:[{id:'cell',kind:'cell',quality:'valid',value_c:25,received_ms:0,age_ms:0}]});
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
 return writes;
}
async function authenticate(page){await page.getByLabel('API bearer token').fill('test-token');await page.getByRole('button',{name:'Refresh',exact:true}).click();}
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
 const write=writes.find(w=>w.path.endsWith('/load/current'));expect(write.path).toBe('/api/v1/devices/load-2/load/current');expect(write.body).toEqual({current_a:.15});expect(write.headers['x-lab-lease']).toBe('lease-1');expect(write.headers['idempotency-key']).toHaveLength(32);
 await page.getByRole('button',{name:'Output ON'}).click();await expect(page.locator('#status')).toContainText('"finalOutput": "on"');await expect(page.getByRole('button',{name:'Output ON'})).toBeDisabled();
});
test('unknown output retains an independent STOP without a lease',async({page})=>{
 const d=device('unavailable-load','dl24m',['output','load.cc']);d.state='fault';d.output='unknown';const writes=await mock(page,[d]);await page.goto('/devices');await authenticate(page);
 await expect(page.getByRole('button',{name:'Output ON'})).toBeDisabled();await page.getByRole('button',{name:'STOP',exact:true}).click();await expect(page.locator('#status')).toContainText('succeeded');
 expect(writes[0].path).toBe('/api/v1/devices/unavailable-load/stop');expect(writes[0].headers['x-lab-lease']).toBeUndefined();
});
test('mixed module panels render without core device-name branches',async({page})=>{
 await mock(page,[device('psu-a','rd60xx',['output','source.cc_cv']),device('paths','gpio_interlock',['output','switch.path','switch.feedback']),device('probe','ds18b20',['temperature.external'])]);await page.goto('/devices');await authenticate(page);
 await expect(page.getByLabel('Source voltage (V)')).toBeVisible();await expect(page.getByRole('button',{name:'Path: isolated'})).toBeEnabled();await page.getByRole('button',{name:'Read temperature channels'}).click();await expect(page.getByText('"kind": "cell"',{exact:false})).toBeVisible();
});
