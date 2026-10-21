import {test,expect} from '@playwright/test';

test('LB-024 integration: DL24 and RD share one authenticated stream without losing independent reservations',async({page})=>{
 const s=await mixedBackend(page),http=[];
 page.on('request',r=>{const path=new URL(r.url()).pathname;if(path.startsWith('/api/v1/')&&!path.startsWith('/api/v1/auth/'))http.push(path);});
 await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');
 await page.locator('#voltageVal').fill('2');await page.locator('#voltageBtn').click();await expect(page.locator('#deviceFeedback')).toContainText('succeeded');
 await page.locator('#deviceSelect').selectOption('load');await expect(page.locator('#reserve')).toHaveText('Release and stop');
 expect(s.sockets).toHaveLength(1);expect(http).toEqual([]);
 expect(s.rpc.some(r=>r.path==='/api/v1/devices/supply/components/source/voltage')).toBe(true);
 expect(s.rpc.some(r=>r.path?.includes('/operations/'))).toBe(false);
 await page.locator('#stopAll').click();await expect(page.locator('#benchStatus')).toContainText('All reserved devices confirmed OFF');
});
import {mixedBackend,openBench,selectReserve} from './bench-multi-fixture.js';
import {openHeaderAction,openAccount} from './bench-controls.js';

test('LB-024: DL24 and RD retain independent reservations and outputs across selection',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');
 await page.locator('#setVal').fill('.15');await page.locator('#setBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 await selectReserve(page,'supply');await page.locator('#voltageVal').fill('12.34');await page.locator('#voltageBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 await page.locator('#loadBtn').click();await expect.poll(()=>s.outputs.supply).toBe('on');
 expect(s.writes.filter(w=>w.path.endsWith('/stop'))).toHaveLength(0);
 await expect.poll(()=>new Set(s.writes.filter(w=>w.path.endsWith('/heartbeat')).map(w=>w.path)).size).toBe(2);
 await page.locator('#deviceSelect').selectOption('load');await expect(page.locator('#reserve')).toHaveText('Release and stop');await expect(page.locator('#setVal')).toHaveValue('.15');
 expect(s.writes.find(w=>w.path.endsWith('/voltage')).body).toEqual({voltage_v:12.34});
 expect(s.writes.find(w=>w.path.endsWith('/voltage')).headers['x-lab-lease']).toBe('lease-supply');
 expect(s.calls.filter(c=>c.path.includes('/supply/dl24m/'))).toHaveLength(0);
 await expect(page.locator('#deviceOverview')).toContainText('supply');await expect(page.locator('#deviceOverview')).toContainText('on');
});

test('LB-024: pending operation stays bound to its device after a dropdown change',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');s.hold.add('load');
 await page.locator('#loadBtn').click();await expect.poll(()=>s.writes.some(w=>w.path.endsWith('/output'))).toBe(true);
 await selectReserve(page,'supply');s.hold.delete('load');await expect.poll(()=>s.outputs.load).toBe('on');
 await expect(page.locator('#loadBtn')).toContainText('OFF');expect(s.outputs.supply).toBe('off');
 await page.locator('#stop').click();await expect.poll(()=>s.writes.some(w=>w.path==='/api/v1/devices/supply/stop')).toBe(true);expect(s.outputs.load).toBe('on');
 await page.locator('#stopAll').click();await expect.poll(()=>s.outputs.load).toBe('off');
});

test('LB-024: all sensors record despite hidden tiles and switching controls',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);
 await expect(page.locator('[data-channel="supply/temperature-external"] [data-value]')).toHaveText('—');
 await openHeaderAction(page,'displayMenu');const choices=page.locator('#channelChoices');
 const tile=choices.getByLabel('supply / input-voltage tile',{exact:true});await tile.check();
 await expect(page.locator('[data-channel="supply/input-voltage"]')).toBeVisible();await tile.uncheck();await page.keyboard.press('Escape');
 await page.locator('#deviceSelect').selectOption('future');await expect(page.locator('#deviceSelect')).toHaveValue('future');
 const download=page.waitForEvent('download');await page.locator('#export').click();const stream=await (await download).createReadStream();let csv='';for await(const part of stream)csv+=part;
 expect(csv).toContain('component_id');expect(csv).toContain('epoch');
 for(const id of ['load','supply','supply-b','meter','future'])expect(csv).toContain(`"${id}"`);
 expect(csv).toContain('"input-voltage"');expect(csv).toContain('"unavailable"');
 expect(s.writes).toHaveLength(0);
});

test('LB-024: unknown actor IDs and sensor-only nodes use generic components',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'future');
 await page.locator('#voltageVal').fill('1.5');await page.locator('#voltageBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 expect(s.writes.some(w=>w.path==='/api/v1/devices/future/components/arbitrary-actor/voltage')).toBe(true);
 await expect(page.locator('#moduleSettings')).toBeHidden();
});
test('LB-024: sensor-only node connects read-only',async({page})=>{
 const s=await mixedBackend(page,{onlyMeters:true});await openBench(page);await expect(page.locator('#reserve')).toBeDisabled();
 await expect(page.locator('[data-channel="meter/voltage"]')).toBeVisible();await expect(page.locator('#export')).toBeEnabled();expect(s.writes).toHaveLength(0);
});

test('LB-024: logout attempts every reserved device after an individual STOP failure',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');s.failStops.add('load');
 await openAccount(page);await page.getByRole('button',{name:'Sign out',exact:true}).click();await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 for(const id of ['load','supply'])expect(s.writes.some(w=>w.path===`/api/v1/devices/${id}/stop`)).toBe(true);
 expect(s.writes.some(w=>w.path==='/api/v1/leases/lease-load/stop')).toBe(false);expect(s.writes.some(w=>w.path==='/api/v1/leases/lease-supply/stop')).toBe(true);
 await expect(page.locator('#benchStatus')).toContainText('Shutdown could not be confirmed');
});

test('LB-024: RD steps, source status and separate second-source settings',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'supply');
 await expect(page.locator('#moduleStatus')).toContainText('CV');await expect(page.locator('#moduleStatus')).toContainText('Protection code: 0');
 await page.locator('#setVal').fill('.123');await page.locator('#setBtn').click();await expect(page.locator('#benchStatus')).toContainText('step 0.01');expect(s.writes.filter(w=>w.path.endsWith('/current'))).toHaveLength(0);
 await page.locator('#setVal').fill('.12');await page.locator('#setBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 await selectReserve(page,'supply-b');await page.locator('#setVal').fill('.34');await page.locator('#setBtn').click();await expect(page.locator('#benchStatus')).toContainText('succeeded');
 expect(s.writes.filter(w=>w.path.endsWith('/current')).map(w=>[w.path,w.body.current_a,w.headers['x-lab-lease']])).toEqual([
  ['/api/v1/devices/supply/components/source/current',.12,'lease-supply'],['/api/v1/devices/supply-b/components/source/current',.34,'lease-supply-b']]);
});

test('LB-024: selected STOP cancels only its queued ON, STOP all cancels both',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');
 let held=false,release;await s.intercept('**/api/v1/node',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});await expect.poll(()=>held).toBe(true);
 await page.locator('#loadBtn').click();await page.locator('#deviceSelect').selectOption('load');await page.locator('#loadBtn').click();
 await page.locator('#stopAll').click();release();await page.unroute('**/api/v1/node');
 await expect.poll(()=>s.writes.filter(w=>w.path.endsWith('/stop')&&w.path.includes('/devices/')).length).toBe(2);
 expect(s.writes.filter(w=>w.path.endsWith('/output'))).toHaveLength(0);
});

test('LB-024: one lost heartbeat or stale instrument does not disable another device',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');
 s.failHeartbeat.add('lease-load');s.stale.add('load');await expect(page.locator('#deviceOverview')).toContainText('Lease lost');
 await expect(page.locator('#setBtn')).toBeEnabled();await page.locator('#deviceSelect').selectOption('load');await expect(page.locator('#setBtn')).toBeDisabled();
 await page.locator('#deviceSelect').selectOption('supply');await expect(page.locator('#setBtn')).toBeEnabled();
});

test('LB-024: multiple actor components retain the command target during confirmation',async({page})=>{
 const s=await mixedBackend(page);s.devices.find(d=>d.id==='future').components.push({id:'second-actor',kind:'source',actions:['voltage','current','output']});
 await openBench(page);await selectReserve(page,'future');s.hold.add('future');await page.locator('#voltageVal').fill('2');await page.locator('#voltageBtn').click();
 await expect.poll(()=>s.writes.some(w=>w.path.endsWith('/voltage'))).toBe(true);const before=s.calls.length;
 await page.locator('#componentSelect').selectOption('second-actor');s.hold.delete('future');
 await expect.poll(()=>s.calls.slice(before).some(c=>c.path==='/api/v1/devices/future/components/arbitrary-actor/state')).toBe(true);
 await expect(page.locator('#voltageBtn')).toBeEnabled();await page.locator('#voltageVal').fill('3');await page.locator('#voltageBtn').click();
 await expect.poll(()=>s.writes.some(w=>w.path==='/api/v1/devices/future/components/second-actor/voltage')).toBe(true);
});

test('LB-024: selected STOP preserves the other device queued ON',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');
 let held=false,release;await s.intercept('**/api/v1/node',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});await expect.poll(()=>held).toBe(true);
 await page.locator('#loadBtn').click();await page.locator('#deviceSelect').selectOption('load');await page.locator('#loadBtn').click();await page.locator('#stop').click();release();await page.unroute('**/api/v1/node');
 await expect.poll(()=>s.outputs.supply).toBe('on');await expect.poll(()=>s.writes.some(w=>w.path==='/api/v1/devices/load/stop')).toBe(true);
 expect(s.writes.filter(w=>w.path.endsWith('/output')).map(w=>w.path)).toEqual(['/api/v1/devices/supply/components/source/output']);
});

test('LB-024: internal navigation confirms OFF and releases every reservation',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');
 await openAccount(page);await page.getByRole('link',{name:'Instruments',exact:true}).click();await expect(page).toHaveURL('/devices');
 for(const id of ['load','supply']){
  const stop=s.writes.findIndex(w=>w.path===`/api/v1/devices/${id}/stop`),release=s.writes.findIndex(w=>w.path===`/api/v1/leases/lease-${id}/stop`);
  expect(stop).toBeGreaterThanOrEqual(0);expect(release).toBeGreaterThan(stop);
 }
});

test('LB-024: STOP all submits every STOP before waiting for a slow device',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');s.hold.add('load');
 await page.locator('#stopAll').click();await expect.poll(()=>s.writes.some(w=>w.path==='/api/v1/devices/supply/stop'),{timeout:2000}).toBe(true);
 s.hold.delete('load');await expect(page.locator('#benchStatus')).toContainText('All reserved devices confirmed OFF');
});

test('LB-024: mixed monitoring stays usable on a narrow screen and exposes curve identities',async({page})=>{
 await mixedBackend(page);await page.setViewportSize({width:390,height:844});await openBench(page);
 const voltage=page.getByRole('button',{name:'supply / voltage (V)',exact:true});
 await expect(voltage).toHaveAttribute('aria-pressed','true');await expect(page.locator('#curveLegend')).toHaveCount(0);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 if(process.env.LABBRIDGE_SCREENSHOTS)await page.screenshot({path:'../.tools/lb023-mobile.png',fullPage:true});
 await page.setViewportSize({width:1440,height:1000});await page.locator('#deviceSelect').selectOption('supply');
 if(process.env.LABBRIDGE_SCREENSHOTS)await page.screenshot({path:'../.tools/lb023-desktop.png',fullPage:true});
});

test('LB-024: discovery cannot expose controls before all initial device state has arrived',async({page})=>{
 const s=await mixedBackend(page);let held=false,release;
 await s.intercept('**/api/v1/devices/meter/readings',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});
 await page.goto('/dl24');await page.getByLabel('Device password',{exact:true}).fill('synthetic password');await page.getByRole('button',{name:'Sign in',exact:true}).click();
 await expect.poll(()=>held).toBe(true);await expect(page.locator('#reserve')).toBeDisabled();await expect(page.locator('#deviceSelect')).toBeDisabled();
 release();await page.unroute('**/api/v1/devices/meter/readings');await expect(page.locator('#connLbl')).toHaveText('Connected');
});

for(const expired of [false,true])test(`LB-024: lost heartbeat cancels that device ON queued behind it (${expired?'inactive response':'HTTP error'})`,async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await selectReserve(page,'supply');
 let held=false,release;await s.intercept('**/api/v1/leases/lease-load/heartbeat',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});
 await expect.poll(()=>held).toBe(true);await page.locator('#deviceSelect').selectOption('load');await page.locator('#loadBtn').click();if(expired)s.leases.get('lease-load').state='expired';else s.failHeartbeat.add('lease-load');release();await page.unroute('**/api/v1/leases/lease-load/heartbeat');
 await expect(page.locator('#deviceOverview')).toContainText('Lease lost');await expect(page.locator('#loadBtn')).toBeDisabled();
 expect(s.writes.filter(w=>w.path==='/api/v1/devices/load/components/sink/output')).toHaveLength(0);
 await page.locator('#deviceSelect').selectOption('supply');await expect(page.locator('#loadBtn')).toBeEnabled();
});

test('LB-024 hardware regression: selected-device drafts restore before a slow subscription acknowledgement',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);
 await page.locator('#setVal').fill('.15');await page.locator('#deviceSelect').selectOption('supply');await expect(page.locator('#benchStatus')).toContainText('Controls selected');
 await page.locator('#setVal').fill('.12');await page.locator('#deviceSelect').selectOption('load');await expect(page.locator('#setVal')).toHaveValue('.15');
 let held=false,release;s.beforeSubscribe=async r=>{if(r.device_id==='supply'){held=true;await new Promise(resolve=>{release=resolve;});}};
 await page.locator('#deviceSelect').selectOption('supply');await expect.poll(()=>held).toBe(true);
 try{await expect(page.locator('#setVal')).toHaveValue('.12',{timeout:1000});await page.locator('#setVal').fill('.18');}
 finally{release();}
 await expect(page.locator('#benchStatus')).toContainText('Controls selected');await expect(page.locator('#setVal')).toHaveValue('.18');
 expect(s.writes).toHaveLength(0);
});
