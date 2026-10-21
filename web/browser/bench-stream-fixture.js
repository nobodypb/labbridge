// The same sanitized API handler serves HTTP recovery and WebSocket RPC.
export async function mockBenchStream(page,state,handle,{devices,operations=()=>[],settings=()=>undefined}){
 state.rpc=[];state.sockets=[];const interceptors=[];
 const matches=(pattern,path)=>new RegExp('^'+pattern.split('*').map(s=>s.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')).join('.*')+'$').test('http://localhost'+path);
 state.intercept=async(pattern,handler)=>{interceptors.push({pattern,handler});await page.route(pattern,handler);};
 await page.routeWebSocket('**/api/v1/ws',ws=>{
  state.sockets.push(ws);if(state.denyWs){ws.close();return;}
  let authenticated=false,selected='',sequence=0,closed=false,ticking=false;
  const call=async(method,path,body={},headers={},intercept=false)=>{
   let result;
   const request={url:()=>`http://localhost${path}`,method:()=>method,postDataJSON:()=>body,headers:()=>({cookie:'LabBridgeSession=synthetic-session','x-lab-csrf':'synthetic-csrf',...headers})};
   const run=async index=>{
    while(index>=0&&!matches(interceptors[index].pattern,path))index--;
    const route={request:()=>request,fulfill:r=>{result={status:r.status??200,body:JSON.parse(r.body),headers:r.headers??{}};},abort:()=>ws.close(),fallback:()=>run(index-1)};
    if(index>=0)await interceptors[index].handler(route);else await handle(route);
   };
   await run(intercept?interceptors.length-1:-1);return result;
  };
  const timer=setInterval(async()=>{
   if(closed||ticking||!authenticated||!selected)return;
   if(state.fail||state.auth.revoked){ws.close();return;}
   ticking=true;
   try{
    const d=devices().find(d=>d.id===selected);if(!d)return;
    const measurement=await call('GET',`/api/v1/devices/${selected}/measurements`),actors=[];
    for(const c of d.components.filter(c=>c.kind!=='sensor')){const a=await call('GET',`/api/v1/devices/${selected}/components/${c.id}/state`);if(a?.body)actors.push({...a.body,component_id:c.id});}
    const ops=[];for(const id of operations()){const op=await call('GET','/api/v1/operations/'+id);if(op?.body)ops.push(op.body);}
    if(!closed)ws.send(JSON.stringify({type:'snapshot',boot_id:state.boot,sequence:++sequence,gap:!!state.gap,body:{device_id:selected,devices:devices(),measurements:measurement?.body,actors,details:settings()?{settings:settings()}:{},operations:ops}}));
    state.gap=false;
   }finally{ticking=false;}
  },100);
  ws.onClose(()=>{closed=true;clearInterval(timer);});
  ws.onMessage(async data=>{
   const r=JSON.parse(String(data));
   if(!authenticated){
    const cookies=await page.context().cookies();
    if(r.type==='auth'&&r.csrf_token==='synthetic-csrf'&&!state.auth.revoked&&cookies.some(c=>c.name==='LabBridgeSession'&&c.value==='synthetic-session')){authenticated=true;ws.send(JSON.stringify({type:'authenticated',boot_id:state.boot}));}
    else{ws.send(JSON.stringify({status:401,body:{detail:'Sign in required'}}));ws.close();}return;
   }
   if(r.type==='subscribe'){if(state.expireDuringSubscribe){state.auth.revoked=true;ws.close();return;}selected=r.device_id;ws.send(JSON.stringify({id:r.id,status:200,body:{subscribed:selected}}));return;}
   state.rpc.push(r);if(state.beforeRpc)await state.beforeRpc(r);
   const headers=Object.fromEntries(Object.entries(r.headers??{}).map(([k,v])=>[k.toLowerCase(),v]));
   const result=await call(r.method,r.path,r.body,headers,true);if(result&&!closed)ws.send(JSON.stringify({id:r.id,...result}));
  });
 });
}
