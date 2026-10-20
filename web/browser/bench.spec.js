import {test,expect} from '@playwright/test';
import {readFileSync} from 'node:fs';
const original=readFileSync(new URL('../vendor/dl24-bench/upstream.html',import.meta.url),'utf8');
async function backend(page,opts={}){
 const state={writes:[],output:'off',boot:'boot-a',sequence:1,fail:false,heartbeatFail:false,settings:{mode:0,cutoff_voltage_v:1,time_limit_s:0,working_brightness:5},...opts};
 const devices=['load-a','load-b'].map(id=>({id,driver:'dl24m',identity:'synthetic-'+id,transport:'ble',state:'ready',output:'off',capabilities:['output','load.cc','dl24m.settings','components'],limits:{voltage_v:25,current_a:.2,power_w:5,max_age_ms:3000},components:[{id:'sink',kind:'sink',actions:['current','output','cutoff','duration']}]}));
 let operation, polls=0;
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname,method=r.method(),body=r.postDataJSON();
  const reply=(data,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)});
  if(state.fail)return route.abort();
  if(r.headers().authorization!=='Bearer bench-test')return reply({detail:'Token required'},401);
  if(method!=='GET')state.writes.push({path,body,headers:r.headers()});
  if(path==='/api/v1/node')return reply({node_id:'test-node',boot_id:state.boot,hostname:'test.local',board:'test',firmware_version:'test',api_version:'1'});
  if(path==='/api/v1/devices')return reply({devices:state.empty?[]:devices.map(d=>({...d,output:state.output}))});
  if(path==='/api/v1/leases')return state.conflict?reply({detail:'Already reserved'},409):reply({id:'lease-a',device_ids:body.device_ids,expires_at_ms:100000,state:'active'});
  if(path.endsWith('/heartbeat'))return state.heartbeatFail?reply({detail:'Lease expired'},423):reply({id:'lease-a',device_ids:['load-a'],expires_at_ms:100000,state:'active'});
  if(path==='/api/v1/leases/lease-a/stop')return reply({id:'lease-a',device_ids:['load-a'],expires_at_ms:0,state:'stopped'});
  if(path.endsWith('/measurements'))return reply({device_id:path.split('/')[4],boot_id:state.boot,sequence:state.sequence++,received_ms:state.sequence*1000,age_ms:0,quality:state.stale?'stale':'valid',voltage_v:10,current_a:.1,power_w:1,temperature_channels:[{id:'internal',kind:'internal',quality:'valid',age_ms:0,received_ms:1000,value_c:25}]});
  if(path.endsWith('/state'))return reply({device_id:path.split('/')[4],component_id:'sink',state:'ready',output:state.output,settings:{current_a:.1,regulation_mode:'cc'}});
  if(path.endsWith('/dl24m/settings')&&method==='GET')return reply(state.settings);
  if(path.includes('/operations/')){
   polls++;if(!state.hold&&polls>=2){operation.status=state.operationFail?'failed':'succeeded';if(operation.status==='succeeded')state.output=operation.final_output;}
   return reply(operation);
  }
  if(method==='POST'||method==='PUT'){
   polls=0;operation={id:'op-a',device_id:path.split('/')[4],status:'running',stage:'readback',read_attempts:1,write_attempts:1,reconnects:0,duration_ms:50,restore_pending:false,final_output:path.endsWith('/output')?(body.enabled?'on':'off'):path.endsWith('/stop')?'off':state.output};
   if(path.endsWith('/dl24m/settings'))Object.assign(state.settings,body);
   return reply(operation,202);
  }
  return reply({detail:'Unmocked '+path},404);
 });return state;
}
async function connect(page){await page.goto('/dl24');await page.getByLabel('API bearer token').fill('bench-test');await page.locator('#connect').click();await expect(page.locator('#connLbl')).toHaveText('Connected');}
test('preserves every original control and unsupported controls never write',async({page})=>{
 const s=await backend(page);await connect(page);
 for(const [,id] of original.matchAll(/\bid="([^"]+)"/g))await expect(page.locator('[id="'+id+'"]')).toHaveCount(1);
 for(const mode of ['cv','cr','cp'])await expect(page.locator('[data-mode="'+mode+'"]')).toBeDisabled();
 await page.locator('#gear').click();
 for(const id of ['calEnable','calVBtn','calIBtn','calTBtn','calCopy','zeroNoLoad','restoreDefaults'])await expect(page.locator('#'+id)).toBeDisabled();
 await expect(page.locator('#unsupported')).toContainText('backend');
 expect(s.writes).toHaveLength(0);await expect(page.locator('#rRPM')).toHaveText('—');
});
test('reserves selected device, confirms commands, stops before switching',async({page})=>{
 const s=await backend(page);await connect(page);await expect(page.locator('#setBtn')).toBeDisabled();
 await page.locator('#reserve').click();await expect(page.locator('#setBtn')).toBeEnabled();
 expect(s.writes.find(w=>w.path==='/api/v1/leases').body.device_ids).toEqual(['load-a']);
 await page.locator('#setVal').fill('.15');await page.locator('#setBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 const write=s.writes.find(w=>w.path.endsWith('/current'));expect(write.body).toEqual({current_a:.15});expect(write.headers['x-lab-lease']).toBe('lease-a');expect(write.headers['idempotency-key']).toHaveLength(32);
 s.hold=true;await page.locator('#loadBtn').click();await expect(page.locator('#loadBtn')).not.toContainText('LOAD ON');
 s.hold=false;await expect(page.locator('#loadBtn')).toContainText('LOAD ON');
 await page.locator('#deviceSelect').selectOption('load-b');await expect(page.locator('#deviceSelect')).toHaveValue('load-b');
 await expect(page.locator('#reserve')).toHaveText('Reserve device');
 const stop=s.writes.findIndex(w=>w.path==='/api/v1/devices/load-a/stop');const release=s.writes.findIndex(w=>w.path==='/api/v1/leases/lease-a/stop');expect(stop).toBeGreaterThan(-1);expect(release).toBeGreaterThan(stop);
});
test('STOP works without lease and settings keep unsupported fields visible',async({page})=>{
 const s=await backend(page);await connect(page);await page.locator('#stop').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 expect(s.writes.find(w=>w.path.endsWith('/stop')).headers['x-lab-lease']).toBeUndefined();
 await page.locator('#reserve').click();await page.locator('#gear').click();await page.locator('#dispBri').fill('7');await page.locator('#dispBriBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 expect(s.writes.find(w=>w.path.endsWith('/settings')).body).toEqual({working_brightness:7});
});
test('lease loss, disconnect and reboot lock control without replaying ON',async({page})=>{
 const s=await backend(page);await connect(page);await page.locator('#reserve').click();s.heartbeatFail=true;
 await expect(page.locator('#benchStatus')).toContainText('Lease');await expect(page.locator('#loadBtn')).toBeDisabled();s.heartbeatFail=false;
 await page.locator('#connect').click();await page.locator('#reserve').click();s.boot='boot-b';
 await expect(page.locator('#benchStatus')).toContainText('restarted');await expect(page.locator('#loadBtn')).toBeDisabled();
 expect(s.writes.filter(w=>w.path.endsWith('/output'))).toHaveLength(0);
});
test('invalid authentication, empty discovery and reservation conflict are explicit',async({page})=>{
 const s=await backend(page);await page.goto('/dl24');await page.locator('#connect').click();await expect(page.locator('#benchStatus')).toContainText('Token');
 s.empty=true;await page.getByLabel('API bearer token').fill('bench-test');await page.locator('#connect').click();await expect(page.locator('#benchStatus')).toContainText('No DL24M');
 s.empty=false;await page.locator('#connect').click();s.conflict=true;await page.locator('#reserve').click();await expect(page.locator('#benchStatus')).toContainText('reserved');await expect(page.locator('#loadBtn')).toBeDisabled();
});
test('responsive themes and CSV work using only local assets',async({page})=>{
 const external=[];page.on('request',r=>{if(!new URL(r.url()).hostname.match(/^(127\.0\.0\.1|localhost)$/))external.push(r.url());});
 await backend(page);await page.setViewportSize({width:390,height:844});await connect(page);
 await page.locator('[data-th="light"]').click();await expect(page.locator('html')).toHaveAttribute('data-theme','light');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 await expect(page.locator('#export')).toBeEnabled();const download=page.waitForEvent('download');await page.locator('#export').click();expect((await download).suggestedFilename()).toMatch(/\.csv$/);expect(external).toEqual([]);
 if(process.env.LABBRIDGE_SCREENSHOTS){await page.screenshot({path:'../.tools/lb020-mobile.png',fullPage:true});await page.setViewportSize({width:1440,height:1000});await page.locator('[data-th="dark"]').click();await page.screenshot({path:'../.tools/lb020-desktop.png',fullPage:true});}
 await page.locator('#gear').click();if(process.env.LABBRIDGE_SCREENSHOTS)await page.screenshot({path:'../.tools/lb020-settings.png',fullPage:true});await page.keyboard.press('Escape');await expect(page.locator('#settingsModal')).toBeHidden();
});
test('network failure and stale telemetry lock ON while independent STOP remains available',async({page})=>{
 const s=await backend(page);await connect(page);await page.locator('#reserve').click();s.stale=true;
 await expect(page.locator('#dataQuality')).toContainText('Stale');await expect(page.locator('#loadBtn')).toBeDisabled();await expect(page.locator('#stop')).toBeEnabled();
 s.fail=true;await expect(page.locator('#benchStatus')).toContainText('Connection lost');await expect(page.locator('#loadBtn')).toBeDisabled();await expect(page.locator('#rV')).toHaveText('—');
 s.fail=false;s.stale=false;await page.locator('#connect').click();await expect(page.locator('#connLbl')).toHaveText('Connected');await expect(page.locator('#loadBtn')).toBeDisabled();
 expect(s.writes.filter(w=>w.path.endsWith('/output'))).toHaveLength(0);
});
test('failed operations remain locked and a failed STOP cannot switch devices',async({page})=>{
 const s=await backend(page);await connect(page);await page.locator('#reserve').click();s.operationFail=true;
 await page.locator('#setVal').fill('.1');await page.locator('#setBtn').click();await expect(page.locator('#benchStatus')).toContainText('failed');await expect(page.locator('#setBtn')).toBeDisabled();
 await page.locator('#deviceSelect').selectOption('load-b');await expect(page.locator('#benchStatus')).toContainText('failed');await expect(page.locator('#deviceSelect')).toHaveValue('load-a');
 expect(s.writes.filter(w=>w.path==='/api/v1/leases/lease-a/stop')).toHaveLength(0);
 s.operationFail=false;await page.locator('#stop').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');await expect(page.locator('#setBtn')).toBeEnabled();
});
test('Load recording does not prevent OFF monitoring or heartbeat',async({page})=>{
 const s=await backend(page);await connect(page);await page.locator('#reserve').click();await page.locator('[data-pm="load"]').click();
 await expect(page.locator('#dataQuality')).toContainText('recording paused');
 await expect.poll(()=>s.writes.filter(w=>w.path.endsWith('/heartbeat')).length).toBeGreaterThan(0);
 s.output='on';await expect(page.locator('#loadBtn')).toContainText('LOAD ON');
 s.output='off';await expect(page.locator('#loadBtn')).toContainText('LOAD OFF');await expect(page.locator('#dataQuality')).toContainText('recording paused');
});
test('source and licence downloads are local and use the right media types',async({request})=>{
 for(const [path,type] of [['/dl24','text/html'],['/dl24.css','text/css'],['/dl24.js','text/javascript'],['/dl24-license.txt','text/plain'],['/dl24-source.tar.gz','application/gzip']]){
  const response=await request.get(path);expect(response.status()).toBe(200);expect(response.headers()['content-type']).toContain(type);
 }
});
test('applying a changed language does not overwrite the selection with cached readback',async({page})=>{
 const s=await backend(page);s.settings.language=0;await connect(page);await page.locator('#reserve').click();await page.locator('#gear').click();
 await page.locator('#langSel').selectOption('1');await page.locator('#langBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 expect(s.writes.find(w=>w.path.endsWith('/settings')).body).toEqual({language:1});
});
