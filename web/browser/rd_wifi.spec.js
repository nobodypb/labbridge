import {test,expect} from '@playwright/test';
async function mock(page){
 let state='idle', configured=false;const requests=[];
 await page.route('**/api/v1/**',async route=>{
  const r=route.request(),path=new URL(r.url()).pathname,body=r.postDataJSON(),method=r.method();
  const send=(value,status=200,headers={})=>route.fulfill({status,headers,contentType:'application/json',body:JSON.stringify(value)});
  if(r.headers().authorization!=='Bearer test-token')return send({type:'about:blank',title:'Unauthorized',status:401,detail:'Token required'},401);
  if(path==='/api/v1/devices')return send({devices:[]});
  if(path==='/api/v1/configuration'){
   if(method==='PUT'){requests.push({path,body,headers:r.headers()});configured=true;return send(body);}
   return send({revision:3,node_id:'test',board:'m5stack-sticks3',devices:[]},200,{ETag:'"3"'});
  }
  if(path.startsWith('/api/v1/rd60xx/provisioning')){
   if(method==='POST'){
    requests.push({path,body});
    if(path.endsWith('/advance'))state=body.expected_phase==='server_address'?'wifi':'complete';
    else if(path.endsWith('/cancel'))state='cancelled';
    else state='server_address';
   }
   return send({state,session_id:'boot-p1',deadline_ms:120000,ap_ssid:'LabBridge-Setup',ap_address:'192.168.4.1',tcp_port:8080,listener_running:true,candidates:state==='complete'?[{serial:'123',model:'RD6018',model_id:60181,firmware_version:136,connected:true,configured}]:[],metrics:{accepted:1,disconnects:0,rejected:0,rx_bytes:45,tx_bytes:8,provision_packets:10}});
  }
  return send({type:'about:blank',title:'Unknown',status:404,detail:'Unmocked route'},404);
 });
 return requests;
}
async function auth(page){await page.goto('/devices');await page.getByLabel('API bearer token').fill('test-token');await page.getByRole('button',{name:'Refresh',exact:true}).click();await page.getByText('RD60xx Wi-Fi setup',{exact:true}).click();}
test('RD pairing requires physical confirmation, advances explicitly and saves serial-bound limits using ETag',async({page})=>{
 const writes=await mock(page);await auth(page);
 await expect(page.getByRole('button',{name:'Start RD pairing',exact:true})).toBeDisabled();
 await page.getByLabel('Target PSU output is OFF').check();await page.getByRole('button',{name:'Start RD pairing',exact:true}).click();
 await expect(page.getByRole('button',{name:'ESP address shown / Connecting WiFi',exact:true})).toBeEnabled();
 await page.getByRole('button',{name:'ESP address shown / Connecting WiFi',exact:true}).click();
 await page.getByRole('button',{name:'Connecting server shown',exact:true}).click();
 await expect(page.getByLabel('Discovered RD source')).toContainText('RD6018');
 await page.getByRole('button',{name:'Add RD source and restart',exact:true}).click();await expect(page.locator('#status')).toContainText('positive');expect(writes.filter(x=>x.path.endsWith('/configuration'))).toHaveLength(0);
 await page.getByLabel('RD device ID').fill('source-a');await page.getByLabel('RD voltage limit (V)').fill('5');await page.getByLabel('RD current limit (A)').fill('.2');await page.getByLabel('RD power limit (W)').fill('1');
 await page.getByRole('button',{name:'Add RD source and restart',exact:true}).click();await expect(page.locator('#status')).toContainText('restarts');
 const saved=writes.find(x=>x.path.endsWith('/configuration'));expect(saved.headers['if-match']).toBe('"3"');expect(saved.body.devices[0]).toMatchObject({id:'source-a',driver:'rd60xx',transport:'tcp',endpoint:'123',rd60xx:{model:6018},limits:{voltage_v:5,current_a:.2,power_w:1}});
 expect(writes.filter(x=>x.path.endsWith('/advance')).map(x=>x.body)).toEqual([{session_id:'boot-p1',expected_phase:'server_address'},{session_id:'boot-p1',expected_phase:'wifi'}]);
});
test('RD pairing can be cancelled independently before creating any device',async({page})=>{
 const writes=await mock(page);await auth(page);await page.getByLabel('Target PSU output is OFF').check();await page.getByRole('button',{name:'Start RD pairing',exact:true}).click();await page.getByRole('button',{name:'Cancel RD pairing',exact:true}).click();
 await expect(page.getByTestId('rd-pair-state')).toContainText('cancelled');expect(writes.at(-1).body).toEqual({session_id:'boot-p1'});expect(writes.some(x=>x.path.endsWith('/configuration'))).toBe(false);
});
