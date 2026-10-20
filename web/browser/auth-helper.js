export async function mockAuth(page, {configured=true, accessPoint=false}={}) {
 const state={configured,accessPoint,password:'synthetic password',revoked:false};
 await page.route('**/api/v1/auth/**',async route=>{
  const r=route.request(), path=new URL(r.url()).pathname, body=r.postDataJSON();
  const signed=!!r.headers().cookie?.includes('LabBridgeSession=synthetic-session')&&!state.revoked;
  const send=(value,status=200,headers={})=>route.fulfill({status,headers,contentType:'application/json',body:JSON.stringify(value)});
  const status=()=>({password_configured:state.configured,setup_allowed:!state.configured&&accessPoint,authenticated:signed,...(signed?{csrf_token:'synthetic-csrf',expires_in_ms:43200000}:{})});
  if(path.endsWith('/status'))return send(status());
  if(path.endsWith('/setup')){
   if(state.configured||!accessPoint)return send({detail:'Setup forbidden'},403);
   state.configured=true;state.password=body.password;return send(status());
  }
  if(path.endsWith('/login')){
   if(!state.configured||body.password!==state.password)return send({detail:'Incorrect password'},401);
   state.revoked=false;return send({...status(),authenticated:true,csrf_token:'synthetic-csrf',expires_in_ms:43200000},200,{'Set-Cookie':'LabBridgeSession=synthetic-session; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict'});
  }
  if(!signed||r.headers()['x-lab-csrf']!=='synthetic-csrf')return send({detail:'Session/CSRF required'},403);
  if(path.endsWith('/password')){if(body.current_password!==state.password)return send({detail:'Incorrect password'},401);state.password=body.password;}
  state.revoked=true;return send({...status(),authenticated:false},200,{'Set-Cookie':'LabBridgeSession=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict'});
 });
 return state;
}
export async function signIn(page) {
 await page.getByLabel('Device password',{exact:true}).fill('synthetic password');
 await page.getByRole('button',{name:'Sign in',exact:true}).click();
}
