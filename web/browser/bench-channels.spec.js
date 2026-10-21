import {test,expect} from '@playwright/test';
import {mixedBackend,openBench} from './bench-multi-fixture.js';
import {openHeaderAction} from './bench-controls.js';

// Observe real Canvas draw calls, without exposing production view internals.
async function observeChart(page){
 await page.addInitScript(()=>{
  const clear=CanvasRenderingContext2D.prototype.clearRect,stroke=CanvasRenderingContext2D.prototype.stroke;
  CanvasRenderingContext2D.prototype.clearRect=function(...args){if(this.canvas.id==='chart')window.chartStrokes=[];return clear.apply(this,args);};
  CanvasRenderingContext2D.prototype.stroke=function(...args){if(this.canvas.id==='chart')window.chartStrokes.push(this.strokeStyle);return stroke.apply(this,args);};
 });
}
async function colour(tile){return tile.locator('.val').evaluate(el=>{const ctx=document.createElement('canvas').getContext('2d');ctx.fillStyle=getComputedStyle(el).color;return ctx.fillStyle;});}
async function isPlotted(page,colour){return page.evaluate(c=>window.chartStrokes.includes(c),colour);}

test('LB-024 channels: all values are shown by default with fewer curves and no extra legend',async({page})=>{
 const s=await mixedBackend(page);await observeChart(page);await openBench(page);
 await expect(page.locator('.channel-tile:not([hidden])')).toHaveCount(21);
 await expect(page.locator('.channel-tile[aria-pressed="true"]')).toHaveCount(10);
 await expect(page.locator('#curveLegend')).toHaveCount(0);
 await expect(page.locator('[data-channel="supply/input-voltage"]')).toBeVisible();
 await expect(page.locator('[data-channel="supply/temperature-external"] [data-value]')).toHaveText('—');
 await expect(page.locator('[data-channel="load/power"]')).toHaveAttribute('aria-pressed','false');
 expect(s.writes).toHaveLength(0);
});

test('LB-024 channels: tile clicks toggle the actual curve and highlight without hiding readings',async({page})=>{
 const s=await mixedBackend(page);await observeChart(page);await page.setViewportSize({width:1440,height:900});await openBench(page);
 const tile=page.locator('[data-channel="load/power"]'),c=await colour(tile);
 expect(await isPlotted(page,c)).toBe(false);
 const inactive=await tile.evaluate(el=>getComputedStyle(el).boxShadow);
 await tile.click();await expect(tile).toHaveAttribute('aria-pressed','true');await expect.poll(()=>isPlotted(page,c)).toBe(true);
 expect(await tile.evaluate(el=>getComputedStyle(el).boxShadow)).not.toBe(inactive);
 await tile.click();await expect(tile).toHaveAttribute('aria-pressed','false');await expect.poll(()=>isPlotted(page,c)).toBe(false);await expect(tile).toBeVisible();await expect(tile.locator('[data-value]')).toHaveText('2.400');
 await tile.focus();await page.keyboard.press('Space');await expect(tile).toHaveAttribute('aria-pressed','true');
 await page.keyboard.press('Enter');await expect(tile).toHaveAttribute('aria-pressed','false');
 expect(s.writes).toHaveLength(0);
});

test('LB-024 channels: values and Canvas strokes share stable channel colours in both themes',async({page})=>{
 const s=await mixedBackend(page);await observeChart(page);await page.setViewportSize({width:1440,height:900});await openBench(page);
 const a=page.locator('[data-channel="load/voltage"]'),b=page.locator('[data-channel="supply/voltage"]');
 const dark=await colour(a);expect(dark).not.toBe(await colour(b));
 for(const theme of ['light','dark']){
  await openHeaderAction(page,'displayMenu');await page.locator(`[data-th="${theme}"]`).click();await page.keyboard.press('Escape');
  for(const tile of [a,b]){const c=await colour(tile);await expect.poll(()=>isPlotted(page,c)).toBe(true);}
 }
 s.devices.reverse();await page.reload();await expect(page.locator('#connLbl')).toHaveText('Connected');expect(await colour(a)).toBe(dark);
});

test('LB-024 channels: Display and tile toggles stay in sync and preserve independent preferences',async({page})=>{
 const s=await mixedBackend(page);await observeChart(page);await page.setViewportSize({width:1440,height:900});await openBench(page);
 const tile=page.locator('[data-channel="load/power"]'),c=await colour(tile);await tile.click();
 await openHeaderAction(page,'displayMenu');const value=page.getByRole('checkbox',{name:'load / power tile',exact:true}),curve=page.getByRole('checkbox',{name:'load / power curve',exact:true});
 await expect(page.locator('#displayDialog')).toContainText('Click a value');await expect(curve).toBeChecked();await value.uncheck();await expect(tile).toBeHidden();
 await expect.poll(()=>isPlotted(page,c)).toBe(true);
 await curve.uncheck();await value.check();await page.keyboard.press('Escape');await expect(tile).toHaveAttribute('aria-pressed','false');
 await tile.click();await openHeaderAction(page,'displayMenu');await expect(curve).toBeChecked();await page.getByRole('checkbox',{name:'load / current tile',exact:true}).uncheck();await page.keyboard.press('Escape');
 await page.locator('#deviceSelect').selectOption('supply');await page.reload();await expect(page.locator('#connLbl')).toHaveText('Connected');
 await expect(tile).toHaveAttribute('aria-pressed','true');await expect(page.locator('[data-channel="load/current"]')).toBeHidden();
 await openHeaderAction(page,'displayMenu');await expect(page.getByRole('checkbox',{name:'load / current curve',exact:true})).toBeChecked();
 expect(s.writes).toHaveLength(0);
});

test.describe('touch',()=>{
test.use({hasTouch:true});
test('LB-024 channels: mobile touch toggles a value without selecting or commanding a device',async({page})=>{
 const s=await mixedBackend(page);await observeChart(page);await page.setViewportSize({width:390,height:844});await openBench(page);
 const tile=page.locator('[data-channel="load/power"]');
 await tile.tap();await expect(tile).toHaveAttribute('aria-pressed','true');
 await expect(page.locator('#deviceSelect')).toHaveValue('load');await expect(page.locator('#devicePanel')).toBeHidden();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);expect(s.writes).toHaveLength(0);
});
});
