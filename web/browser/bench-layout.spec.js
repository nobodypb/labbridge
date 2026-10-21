import {test,expect} from '@playwright/test';
import {mixedBackend,openBench,selectReserve} from './bench-multi-fixture.js';
import {openHeaderAction,openDevicePanel} from './bench-controls.js';

test('LB-024 layout: content-sized device groups share rows and reflow when channels are hidden',async({page})=>{
 const s=await mixedBackend(page);s.devices.splice(1,3);await page.setViewportSize({width:1440,height:900});await openBench(page);
 await expect(page.locator('.measurement-strip')).toHaveCount(2);
 const load=page.locator('.measurement-strip').filter({has:page.locator('[data-channel="load/voltage"]')}),supply=page.locator('.measurement-strip').filter({has:page.locator('[data-channel="meter/voltage"]')});
 const first=await load.boundingBox(),next=await supply.boundingBox();
 expect(first.width).toBeLessThan(410);expect(next.y).toBe(first.y);expect(next.x).toBeGreaterThanOrEqual(first.x+first.width);
 await openHeaderAction(page,'displayMenu');await page.getByRole('checkbox',{name:'load / power tile',exact:true}).uncheck();await page.keyboard.press('Escape');
 expect((await load.boundingBox()).width).toBeLessThan(first.width-50);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});

test('LB-024 layout: dock can be dragged larger and smaller and keeps its preferred height',async({page})=>{
 await mixedBackend(page);await page.setViewportSize({width:1280,height:720});await openBench(page);
 const handle=page.getByRole('separator',{name:'Resize measurement area'}),dock=page.locator('#measurementDock'),chart=page.locator('#chart');
 const original=await dock.boundingBox(),graph=await chart.boundingBox(),grip=await handle.boundingBox();
 await page.mouse.move(grip.x+grip.width/2,grip.y+grip.height/2);await page.mouse.down();await page.mouse.move(grip.x+grip.width/2,grip.y-130,{steps:8});await page.mouse.up();
 const enlarged=await dock.boundingBox();expect(enlarged.height).toBeGreaterThan(original.height+120);expect((await chart.boundingBox()).height).toBeLessThan(graph.height-120);
 await page.reload();await expect(page.locator('#connLbl')).toHaveText('Connected');expect((await dock.boundingBox()).height).toBeCloseTo(enlarged.height,0);
 const moved=await handle.boundingBox();await page.mouse.move(moved.x+20,moved.y+5);await page.mouse.down();await page.mouse.move(moved.x+20,moved.y+1000);await page.mouse.up();
 expect((await dock.boundingBox()).height).toBeGreaterThanOrEqual(64);expect((await dock.boundingBox()).height).toBeLessThanOrEqual(80);await expect(handle).toBeVisible();
});

test('LB-024 layout: keyboard dock resizing is bounded and adapts to viewport height',async({page})=>{
 await mixedBackend(page);await page.setViewportSize({width:1280,height:720});await openBench(page);
 const handle=page.getByRole('separator',{name:'Resize measurement area'}),dock=page.locator('#measurementDock');await handle.focus();
 const initial=(await dock.boundingBox()).height;await page.keyboard.press('ArrowUp');expect((await dock.boundingBox()).height).toBeCloseTo(initial+20,0);
 await page.keyboard.press('ArrowDown');expect((await dock.boundingBox()).height).toBeCloseTo(initial,0);
 await page.keyboard.press('End');expect((await dock.boundingBox()).height).toBeGreaterThan(300);expect((await page.locator('#chart').boundingBox()).height).toBeGreaterThanOrEqual(158);
 await page.setViewportSize({width:1280,height:480});await expect.poll(async()=>Number(await handle.getAttribute('aria-valuenow'))).toBeLessThanOrEqual(212);
 expect((await page.locator('#chart').boundingBox()).height).toBeGreaterThanOrEqual(158);const small=await dock.boundingBox();expect(small.y+small.height).toBeLessThanOrEqual(480);
 await page.keyboard.press('Home');expect((await dock.boundingBox()).height).toBeGreaterThanOrEqual(64);expect((await dock.boundingBox()).height).toBeLessThanOrEqual(80);
});

test('LB-024 layout: mobile groups and expanded channel sets fit the dock without horizontal scrolling',async({page})=>{
 await mixedBackend(page);await page.setViewportSize({width:390,height:844});await openBench(page);
 await openHeaderAction(page,'displayMenu');for(const input of await page.locator('#channelChoices input[aria-label$=" tile"]').all())await input.check();await page.keyboard.press('Escape');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 expect(await page.locator('#measurementContent').evaluate(el=>el.scrollWidth<=el.clientWidth)).toBe(true);
 const handle=page.getByRole('separator',{name:'Resize measurement area'});await handle.focus();await page.keyboard.press('End');
 await expect(page.locator('[data-channel="supply/temperature-external"]')).toContainText('—');
 if(process.env.LABBRIDGE_SCREENSHOTS)await page.screenshot({path:'../.tools/lb024-flex-mobile.png'});
});

test('LB-024 layout: touch dragging resizes the dock and releases the gesture cleanly',async({page})=>{
 const s=await mixedBackend(page);await page.setViewportSize({width:390,height:844});await openBench(page);
 const cdp=await page.context().newCDPSession(page);await cdp.send('Emulation.setTouchEmulationEnabled',{enabled:true});
 const handle=page.getByRole('separator',{name:'Resize measurement area'}),box=await handle.boundingBox(),initial=(await page.locator('#measurementDock').boundingBox()).height;
 const x=Math.round(box.x+box.width/2),y=Math.round(box.y+box.height/2);
 await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y}]});
 for(const offset of [30,60,90,120])await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x,y:y-offset}]});
 await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
 expect((await page.locator('#measurementDock').boundingBox()).height).toBeGreaterThan(initial+100);
 await expect(page.locator('body')).not.toHaveClass(/measurement-resizing/);expect(s.writes).toHaveLength(0);
});

for(const [width,height,minGraph] of [[1280,720,420],[1440,900,600]])test(`LB-024 layout: ${width}x${height} keeps a 56px header and a large graph with five devices`,async({page})=>{
 const s=await mixedBackend(page,{waveform:true});await page.setViewportSize({width,height});await openBench(page);
 await expect(page.locator('[data-channel="meter/voltage"]')).toBeVisible();
 const boxes=await page.evaluate(()=>{
  const box=q=>{const r=document.querySelector(q).getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height,bottom:r.bottom};};
  return {header:box('#benchHeader'),chart:box('#chart'),dock:box('#measurementDock'),side:box('#devicePanel'),body:document.documentElement.scrollWidth};
 });
 expect(boxes.header.height).toBeLessThanOrEqual(56);expect(boxes.chart.height).toBeGreaterThanOrEqual(minGraph);
 expect(boxes.chart.y).toBeLessThanOrEqual(96);expect(boxes.dock.height).toBeLessThanOrEqual(144);expect(boxes.body).toBeLessThanOrEqual(width);
 for(const id of ['deviceSelect','componentSelect','reserve','stop','gear','setBtn','voltageBtn','loadBtn'])await expect(page.locator(`#devicePanel #${id}`)).toHaveCount(1);
 for(const id of ['connect','stopAll','recordingMenu','displayMenu','accountMenu','statusMenu'])await expect(page.locator(`#benchHeader #${id}`)).toHaveCount(1);
 await expect(page.locator('#stopAll')).toBeVisible();expect(s.writes).toHaveLength(0);
 for(const tile of await page.locator('.channel-tile:not([hidden])').all())expect((await tile.boundingBox()).height).toBeLessThanOrEqual(44);
 if(process.env.LABBRIDGE_SCREENSHOTS){await expect.poll(()=>s.calls.filter(c=>c.path.endsWith('/readings')).length).toBeGreaterThanOrEqual(15);await page.screenshot({path:`../.tools/lb024-layout-${width}.png`});console.log(JSON.stringify({viewport:[width,height],header:boxes.header.height,canvas:boxes.chart.height,dock:boxes.dock.height}));}
});

test('LB-024 layout: menus and long names do not push down the plot',async({page})=>{
 const s=await mixedBackend(page);for(const [i,d] of s.devices.entries()){d.id=`instrument-with-a-long-configured-name-${i}`;s.outputs[d.id]='off';}
 await page.setViewportSize({width:1280,height:720});await openBench(page);
 const before=await page.locator('#chart').boundingBox();
 await openHeaderAction(page,'displayMenu');await expect(page.locator('#channelChoices')).toBeVisible();
 expect((await page.locator('#benchHeader').boundingBox()).height).toBeLessThanOrEqual(56);
 expect((await page.locator('#chart').boundingBox()).height).toBe(before.height);
 await page.keyboard.press('Escape');await expect(page.locator('#displayDialog')).toBeHidden();await expect(page.locator('#displayMenu')).toBeFocused();
 await openHeaderAction(page,'recordingMenu');await expect(page.locator('#clearBtn')).toBeVisible();
 await page.keyboard.press('Escape');await expect(page.locator('#recordingMenu')).toBeFocused();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});

test('LB-024 layout: mobile starts with graph visible and opens device controls on the left',async({page})=>{
 await mixedBackend(page);await page.setViewportSize({width:390,height:844});await openBench(page);
 await expect(page.locator('#devicePanel')).toBeHidden();await expect(page.locator('#stopAll')).toBeVisible();
 expect((await page.locator('#benchHeader').boundingBox()).height).toBeLessThanOrEqual(56);
 expect((await page.locator('#chart').boundingBox()).y).toBeLessThanOrEqual(96);expect((await page.locator('#chart').boundingBox()).height).toBeGreaterThan(400);
 await openDevicePanel(page);await expect(page.locator('#deviceSelect')).toBeVisible();await selectReserve(page,'supply');await expect(page.locator('#voltageVal')).toBeVisible();
 await page.keyboard.press('Escape');await expect(page.locator('#devicePanel')).toBeHidden();
 await openHeaderAction(page,'displayMenu');await page.locator('[data-th="light"]').click();await expect(page.locator('html')).toHaveAttribute('data-theme','light');await page.keyboard.press('Escape');
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 if(process.env.LABBRIDGE_SCREENSHOTS)await page.screenshot({path:'../.tools/lb024-layout-mobile.png'});
});

test('LB-024 layout: authentication and error details never enlarge the header',async({page})=>{
 const s=await mixedBackend(page);await page.goto('/dl24');await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 expect((await page.locator('#benchHeader').boundingBox()).height).toBeLessThanOrEqual(56);
 await page.getByLabel('Device password',{exact:true}).fill('wrong password');await page.getByRole('button',{name:'Sign in',exact:true}).click();
 await expect(page.locator('#authFeedback')).toContainText('Incorrect');
 await page.getByLabel('Device password',{exact:true}).fill('synthetic password');await page.getByRole('button',{name:'Sign in',exact:true}).click();await expect(page.locator('#accountDialog')).toBeHidden();
 await expect(page.locator('#connLbl')).toHaveText('Connected');await selectReserve(page,'load');s.failHeartbeat.add('lease-load');
 await expect(page.locator('#deviceFeedback')).toContainText('Lease lost');
 await expect(page.locator('#statusMenu')).toHaveClass(/has-error/);expect((await page.locator('#benchHeader').boundingBox()).height).toBeLessThanOrEqual(56);
});


test('LB-024 layout: a single read-only device keeps the same compact shell',async({page})=>{
 await mixedBackend(page,{onlyMeters:true});await page.setViewportSize({width:1440,height:900});await openBench(page);
 expect((await page.locator('#benchHeader').boundingBox()).height).toBeLessThanOrEqual(56);
 expect((await page.locator('#chart').boundingBox()).height).toBeGreaterThanOrEqual(600);
 await expect(page.locator('.measurement-strip')).toHaveCount(1);await expect(page.locator('#reserve')).toBeDisabled();
});

test('LB-024 layout: global STOP stays reachable while device settings are open',async({page})=>{
 const s=await mixedBackend(page);await openBench(page);await selectReserve(page,'load');await page.locator('#gear').click();
 await expect(page.locator('#settingsModal')).toBeVisible();await page.locator('#stopAll').click({timeout:2000});
 await expect.poll(()=>s.writes.some(w=>w.path==='/api/v1/devices/load/stop')).toBe(true);
 await expect(page.locator('#benchStatus')).toContainText('All reserved devices confirmed OFF');
});

test('LB-024 layout: command feedback does not show an earlier success during a pending STOP',async({page})=>{
 await mixedBackend(page);await openBench(page);await selectReserve(page,'load');
 await page.locator('#setVal').fill('.15');await page.locator('#setBtn').click();await expect(page.locator('#deviceFeedback')).toContainText('succeeded');
 let held=false,release;await page.route('**/api/v1/devices/load/stop',async route=>{held=true;await new Promise(r=>{release=r;});await route.fallback();});
 await page.locator('#stop').click();await expect.poll(()=>held).toBe(true);await expect(page.locator('#deviceFeedback')).not.toContainText('succeeded',{timeout:1000});
 release();await expect(page.locator('#deviceFeedback')).toContainText('succeeded');
});
