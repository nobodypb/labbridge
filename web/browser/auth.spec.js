import {test,expect} from '@playwright/test';
import {mockAuth,signIn} from './auth-helper.js';
async function backend(page,{accessPoint=false}={}){
 const requests=[];
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname;
  requests.push({path,method:r.method(),headers:r.headers()});
  const signed=r.headers().cookie?.includes('LabBridgeSession=synthetic-session');
  if(!signed&&!(accessPoint&&path.startsWith('/api/v1/wifi')))return route.fulfill({status:401,contentType:'application/problem+json',body:JSON.stringify({detail:'Sign in required'})});
  const body=path.endsWith('/scan')?{state:'complete',networks:[{ssid:'Bench network',rssi_dbm:-40,secured:true}]}:{devices:[]};
  return route.fulfill({contentType:'application/json',body:JSON.stringify(body)});
 });
 return requests;
}
test('LAN scan prompts for password and resumes automatically; reload and navigation retain the session',async({page})=>{
 const requests=await backend(page);await mockAuth(page);await page.goto('/wifi');
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 await signIn(page);await expect(page.getByLabel('Network name (SSID)')).toHaveValue('Bench network');
 await page.reload();await expect(page.getByLabel('Network name (SSID)')).toHaveValue('Bench network');
 await page.getByRole('link',{name:'Instruments',exact:true}).click();await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
 expect(requests.filter(r=>r.method!=='GET')).toHaveLength(0);
 await page.getByRole('button',{name:'Sign out',exact:true}).click();await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
});
test('AP scan needs no login and first password requires matching confirmation',async({page})=>{
 await backend(page,{accessPoint:true});await mockAuth(page,{configured:false,accessPoint:true});await page.goto('/wifi');
 await expect(page.getByLabel('Network name (SSID)')).toHaveValue('Bench network');
 await page.getByLabel('New device password',{exact:true}).fill('synthetic password');
 await page.getByLabel('Confirm device password',{exact:true}).fill('different password');
 await page.getByRole('button',{name:'Set password',exact:true}).click();await expect(page.locator('#status')).toContainText('match');
 await page.getByLabel('Confirm device password',{exact:true}).fill('synthetic password');
 await page.getByRole('button',{name:'Set password',exact:true}).click();await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
});
test('LAN first setup explains AP or USB and never offers an open password setter',async({page})=>{
 await backend(page);await mockAuth(page,{configured:false});await page.goto('/devices');
 await expect(page.getByText(/192\.168\.4\.1/)).toBeVisible();await expect(page.getByText(/ui-password/)).toBeVisible();
 await expect(page.getByLabel('New device password',{exact:true})).toHaveCount(0);
});
test('wrong password stays in the login form without revealing it',async({page})=>{
 await backend(page);await mockAuth(page);await page.goto('/devices');await page.getByLabel('Device password',{exact:true}).fill('incorrect password');
 await page.getByRole('button',{name:'Sign in',exact:true}).click();await expect(page.locator('#status')).toContainText('Incorrect password');
 await expect(page.locator('#status')).not.toContainText('incorrect password');await expect(page.getByLabel('Device password',{exact:true})).toHaveValue('');
});
test('password replacement invalidates the browser session and requires a new login',async({page})=>{
 await backend(page);await mockAuth(page);await page.goto('/devices');await signIn(page);
 await page.locator('summary').filter({hasText:'Change password'}).click();
 await page.getByLabel('Current device password',{exact:true}).fill('synthetic password');
 await page.getByLabel('New device password',{exact:true}).fill('replacement password');
 await page.getByLabel('Confirm device password',{exact:true}).fill('replacement password');
 await page.locator('form').getByRole('button',{name:'Change password',exact:true}).click();
 await expect(page.getByRole('heading',{name:'Sign in',exact:true})).toBeVisible();
 await expect(page.locator('#status')).toContainText('Password changed');
 await page.getByLabel('Device password',{exact:true}).fill('replacement password');
 await page.getByRole('button',{name:'Sign in',exact:true}).click();await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
});
for(const path of ['/','/wifi','/devices','/dl24','/dl24/'])test(`password sign-in works on user entry point ${path}`,async({page})=>{
 const requests=await backend(page);await mockAuth(page);
 await page.route('**/api/v1/node',route=>route.fulfill({contentType:'application/json',body:JSON.stringify({node_id:'test-node',boot_id:'test-boot',hostname:'test.local',board:'test',firmware_version:'test',api_version:'1'})}));
 await page.goto(path);
 await expect(page.getByLabel('Device password',{exact:true})).toHaveAttribute('autocomplete','current-password');
 await expect(page.getByLabel(/API bearer token|Management token/i)).toHaveCount(0);
 await signIn(page);await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
 await page.reload();await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
 expect(requests.filter(r=>r.method!=='GET')).toHaveLength(0);
});
for(const afterLogin of [false,true])test(`delayed unauthenticated scan cannot erase ${afterLogin?'a new login':'password-manager input'}`,async({page})=>{
 await backend(page);await mockAuth(page);let held=false,release;
 await page.route('**/api/v1/wifi/scan',async route=>{
  if(!held&&!route.request().headers().cookie?.includes('LabBridgeSession=')){
   held=true;await new Promise(r=>{release=r;});
   return route.fulfill({status:401,contentType:'application/problem+json',body:JSON.stringify({detail:'Sign in required'})});
  }
  return route.fallback();
 });
 await page.goto('/wifi');await expect.poll(()=>held).toBe(true);
 await page.getByLabel('Device password',{exact:true}).fill('synthetic password');
 if(afterLogin){
  await page.getByRole('button',{name:'Sign in',exact:true}).click();
  await expect(page.getByLabel('Network name (SSID)')).toHaveValue('Bench network');
 }
 const rejected=page.waitForResponse(r=>new URL(r.url()).pathname==='/api/v1/wifi/scan'&&r.status()===401);release();await rejected;
 if(!afterLogin){
  await expect(page.getByLabel('Device password',{exact:true})).toHaveValue('synthetic password');
  await page.getByRole('button',{name:'Sign in',exact:true}).click();
 }
 await expect(page.getByRole('button',{name:'Sign out',exact:true})).toBeVisible();
 await expect(page.locator('#status')).toHaveText('1 networks found');
});
