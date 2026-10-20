import {test,expect} from '@playwright/test';
import {readFileSync} from 'node:fs';
import {mockAuth,signIn} from './auth-helper.js';
const original=readFileSync(new URL('../vendor/dl24-bench/upstream.html',import.meta.url),'utf8');
async function backend(page,opts={}){
 let auth;
 const state={calls:[],writes:[],output:'off',boot:'boot-a',sequence:1,fail:false,heartbeatFail:false,settings:{mode:0,cutoff_voltage_v:1,time_limit_s:0,working_brightness:5},...opts};
 const devices=['load-a','load-b'].map(id=>({id,driver:'dl24m',identity:'synthetic-'+id,transport:'ble',state:'ready',output:'off',capabilities:['output','load.cc','dl24m.settings','components'],limits:{voltage_v:25,current_a:.2,power_w:5,max_age_ms:3000},components:[{id:'sink',kind:'sink',actions:['current','output','cutoff','duration']}]}));
 let operation, polls=0;
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname,method=r.method(),body=r.postDataJSON();
  const reply=(data,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)});
  if(state.fail)return route.abort();
  state.calls.push({path,method,headers:r.headers()});
  const signed=r.headers().cookie?.includes('LabBridgeSession=synthetic-session')&&!auth.revoked;
  if(!signed)return reply({detail:'Sign in required'},401);
  if(signed&&method!=='GET'&&r.headers()['x-lab-csrf']!=='synthetic-csrf')return reply({detail:'CSRF required'},403);
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
 });auth=await mockAuth(page,opts.auth);state.auth=auth;return state;
}
async function connect(page){await passwordConnect(page);}
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
 const s=await backend(page);await page.goto('/dl24');
 await page.getByLabel('Device password',{exact:true}).fill('incorrect password');await page.getByRole('button',{name:'Sign in',exact:true}).click();await expect(page.locator('#benchStatus')).toContainText('Incorrect password');
 s.empty=true;await signIn(page);await expect(page.locator('#benchStatus')).toContainText('No DL24M');
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
test('connects and monitors with a two-request transport budget',async({page})=>{
 await backend(page);let active=0,peak=0;
 await page.route('**/api/v1/**',async route=>{
  active++;peak=Math.max(peak,active);
  try{if(active>2)return await route.abort('connectionreset');await new Promise(r=>setTimeout(r,50));await route.fallback();}
  finally{active--;}
 });
 await connect(page);await expect(page.locator('#dataQuality')).toContainText('fresh');expect(peak).toBeLessThanOrEqual(2);
});
test('shares one API connection across monitoring, heartbeat and command confirmation',async({page})=>{
 const s=await backend(page);let active=0,peak=0;
 await page.route('**/api/v1/**',async route=>{
  active++;peak=Math.max(peak,active);
  try{if(active>1)return await route.abort('connectionreset');await new Promise(r=>setTimeout(r,120));await route.fallback();}
  finally{active--;}
 });
 await connect(page);await page.locator('#reserve').click();await expect(page.locator('#setBtn')).toBeEnabled();
 s.hold=true;await page.locator('#setVal').fill('.1');await page.locator('#setBtn').click();
 await expect.poll(()=>s.writes.some(w=>w.path.endsWith('/heartbeat'))).toBe(true);
 s.hold=false;await expect(page.locator('#benchStatus')).toContainText('succeeded');expect(peak).toBe(1);
});
test('priority STOP cancels an ON request still waiting for the API connection',async({page})=>{
 const s=await backend(page);await connect(page);await page.locator('#reserve').click();await expect(page.locator('#loadBtn')).toBeEnabled();
 let held=false,release;
 await page.route('**/api/v1/node',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});
 await expect.poll(()=>held).toBe(true);
 await page.locator('#loadBtn').click();await page.locator('#stop').click();release();
 await expect(page.locator('#benchStatus')).toContainText('succeeded');
 expect(s.writes.filter(w=>w.path.endsWith('/output'))).toHaveLength(0);
 await expect(page.locator('#loadBtn')).toContainText('LOAD OFF');
});

async function passwordConnect(page){
 await page.goto('/dl24');await signIn(page);
 await expect(page.locator('#connLbl')).toHaveText('Connected');
}
test('Bench password: login authorizes cookie-only reservation, command and independent STOP',async({page})=>{
 const s=await backend(page);await passwordConnect(page);
 await expect(page.getByLabel('API bearer token')).toHaveCount(0);
 expect(s.writes).toHaveLength(0);
 await page.locator('#reserve').click();await expect(page.locator('#setBtn')).toBeEnabled();
 await page.locator('#setVal').fill('.15');await page.locator('#setBtn').click();
 await expect(page.locator('#benchStatus')).toContainText('succeeded');
 await page.locator('#stop').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 await expect.poll(()=>s.writes.some(w=>w.path.endsWith('/heartbeat'))).toBe(true);
 for(const w of s.writes){expect(w.headers.authorization).toBeUndefined();expect(w.headers['x-lab-csrf']).toBe('synthetic-csrf');}
 const stop=s.writes.find(w=>w.path==='/api/v1/devices/load-a/stop');expect(stop.headers['x-lab-lease']).toBeUndefined();
});
test('Bench password: reload and navigation share the existing UI session without reserving',async({page})=>{
 const s=await backend(page);await page.goto('/devices');await signIn(page);
 await page.getByRole('link',{name:'DL24M Bench',exact:true}).click();
 await expect(page.locator('#connLbl')).toHaveText('Connected');
 await page.reload();await expect(page.locator('#connLbl')).toHaveText('Connected');
 await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
 expect(s.writes).toHaveLength(0);
 await page.getByRole('link',{name:'Instruments',exact:true}).click();
 await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
});
test('Bench password: an expired session cancels ON and login never replays or reserves',async({page})=>{
 const s=await backend(page);await passwordConnect(page);await page.locator('#reserve').click();
 await expect(page.locator('#loadBtn')).toBeEnabled();let attempts=0;
 await page.route('**/api/v1/devices/*/components/*/output',async route=>{attempts++;s.auth.revoked=true;await route.fulfill({status:401,contentType:'application/json',body:JSON.stringify({detail:'Session expired'})});});
 await page.locator('#loadBtn').click();
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 await expect(page.locator('#loadBtn')).toBeDisabled();
 const reservations=s.writes.filter(w=>w.path==='/api/v1/leases').length;
 await signIn(page);await expect(page.locator('#connLbl')).toHaveText('Connected');
 await expect(page.locator('#reserve')).toHaveText('Reserve device');await expect(page.locator('#loadBtn')).toBeDisabled();
 expect(attempts).toBe(1);expect(s.writes.filter(w=>w.path==='/api/v1/leases')).toHaveLength(reservations);
});
for(const failed of [false,true])test(`Bench password: sign out ${failed?'reports unconfirmed STOP':'confirms STOP and release'} before ending the session`,async({page})=>{
 const s=await backend(page);await passwordConnect(page);await page.locator('#reserve').click();
 await expect(page.locator('#setBtn')).toBeEnabled();s.operationFail=failed;
 const calls=[];page.on('request',r=>{if(r.method()!=='GET')calls.push(new URL(r.url()).pathname);});
 await page.getByRole('button',{name:'Sign out',exact:true}).click();
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 expect(calls[0]).toBe('/api/v1/devices/load-a/stop');expect(calls.at(-1)).toBe('/api/v1/auth/session');
 if(failed){expect(calls).not.toContain('/api/v1/leases/lease-a/stop');await expect(page.locator('#benchStatus')).toContainText('Shutdown could not be confirmed');}
 else expect(calls.indexOf('/api/v1/leases/lease-a/stop')).toBeGreaterThan(0);
 await expect(page.locator('#loadBtn')).toBeDisabled();
});
test('Bench password: sign out cancels an ON command queued behind a read',async({page})=>{
 const s=await backend(page);await passwordConnect(page);await page.locator('#reserve').click();await expect(page.locator('#loadBtn')).toBeEnabled();
 let held=false,release;
 await page.route('**/api/v1/node',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});
 await expect.poll(()=>held).toBe(true);
 await page.locator('#loadBtn').click();await page.getByRole('button',{name:'Sign out',exact:true}).click();release();
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 expect(s.writes.filter(w=>w.path.endsWith('/output'))).toHaveLength(0);
});
test('Bench password: password change leaves independent STOP and heartbeat reachable',async({page})=>{
 const s=await backend(page);await passwordConnect(page);await page.locator('#reserve').click();await expect(page.locator('#setBtn')).toBeEnabled();
 let held=false,release;
 await page.route('**/api/v1/auth/password',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});
 await page.locator('#benchAuth summary').click();
 await page.getByLabel('Current device password',{exact:true}).fill('synthetic password');
 await page.getByLabel('New device password',{exact:true}).fill('replacement password');
 await page.getByLabel('Confirm device password',{exact:true}).fill('replacement password');
 await page.getByRole('button',{name:'Change password',exact:true}).click();await expect.poll(()=>held).toBe(true);
 await page.locator('#stop').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 await expect.poll(()=>s.writes.some(w=>w.path.endsWith('/heartbeat'))).toBe(true);
 release();await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();await expect(page.locator('#loadBtn')).toBeDisabled();
});
for(const accessPoint of [false,true])test(`Bench password: first setup on ${accessPoint?'AP':'LAN'} never grants unauthenticated laboratory access`,async({page})=>{
 const s=await backend(page,{auth:{configured:false,accessPoint}});await page.goto('/dl24');
 await expect(page.getByRole('heading',{name:'Set device password'})).toBeVisible();
 if(accessPoint){
  await page.getByLabel('New device password',{exact:true}).fill('synthetic password');
  await page.getByLabel('Confirm device password',{exact:true}).fill('synthetic password');
  await page.getByRole('button',{name:'Set password',exact:true}).click();
  await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 }else{await expect(page.getByText(/192\.168\.4\.1/)).toBeVisible();await expect(page.getByLabel('New device password',{exact:true})).toHaveCount(0);}
 expect(s.calls).toHaveLength(0);await expect(page.locator('#reserve')).toBeDisabled();
});
test('Bench password: discovery expires without reporting a connected session',async({page})=>{
 const s=await backend(page);
 await page.route('**/api/v1/devices/*/measurements',async route=>{s.auth.revoked=true;await route.fulfill({status:401,contentType:'application/json',body:JSON.stringify({detail:'Session expired'})});});
 await page.goto('/dl24');await signIn(page);await expect.poll(()=>s.auth.revoked).toBe(true);
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 await expect(page.locator('#benchStatus')).not.toHaveText(/^Connected\./);
 await expect(page.locator('#reserve')).toBeDisabled();
});
