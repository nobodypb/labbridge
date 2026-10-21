// SPDX-License-Identifier: GPL-3.0-or-later
// One authenticated connection, USB-compatible RPC. Writes are never replayed.
export class RpcStream {
 constructor(url,Socket=WebSocket){this.url=url;this.Socket=Socket;this.pending=new Map();this.serial=0;this.ready=false;this.boot='';this.sequence=0;this.onSnapshot=(_event)=>{};this.onGap=()=>{};this.onLoss=(_error)=>{};}
 async open(token){
  this.close();this.sequence=0;
  return new Promise((resolve,reject)=>{
   const socket=new this.Socket(this.url);this.socket=socket;let done=false;
   const timer=setTimeout(()=>fail(Error('Connection lost: authentication timed out')),3000);
   const fail=error=>{if(!done){done=true;clearTimeout(timer);reject(error);}if(this.socket===socket)this.lost(error);socket.onclose=socket.onerror=socket.onmessage=null;socket.close();};
   socket.onopen=()=>socket.send(JSON.stringify({type:'auth',...(typeof token==='string'?{token}:token)}));
   socket.onerror=()=>fail(Error('Connection lost: WebSocket unavailable'));
   socket.onclose=()=>fail(Error('Connection lost: reconnect and inspect state before repeating commands'));
   socket.onmessage=event=>{
    try{
     const message=JSON.parse(event.data);
     if(!done){
      if(message.type!=='authenticated')return fail(Error(message.body?.detail??'Token authentication failed'));
      done=true;clearTimeout(timer);this.boot=message.boot_id;this.ready=true;resolve(this.boot);return;
     }
     if(message.type==='snapshot'){
      if(message.boot_id!==this.boot)return fail(Error('ESP restarted: reservation invalidated; reconnect explicitly'));
      if(message.sequence<=this.sequence)return;
      if(message.gap||this.sequence&&message.sequence!==this.sequence+1)this.onGap();
      this.sequence=message.sequence;this.onSnapshot(message);return;
     }
     const job=this.pending.get(message.id);
     if(job){this.pending.delete(message.id);clearTimeout(job.timer);job.resolve(message);}
    }catch(error){fail(error instanceof Error?error:Error('Invalid WebSocket response'));}
   };
  });
 }
 lost(error){
  this.ready=false;
  for(const job of this.pending.values()){clearTimeout(job.timer);job.reject(error);}
  this.pending.clear();this.onLoss(error);
 }
 close(){
  const socket=this.socket;this.socket=undefined;this.ready=false;
  if(socket){socket.onclose=socket.onerror=socket.onmessage=socket.onopen=null;socket.close();}
  for(const job of this.pending.values()){clearTimeout(job.timer);job.reject(Error('Connection changed; pending command was not replayed'));}this.pending.clear();
 }
 request(message){
  if(!this.ready)return Promise.reject(Error('An authenticated WebSocket is required'));
  if(this.pending.size>=8)return Promise.reject(Error('WebSocket request buffer full'));
  const id=String(++this.serial);
  return new Promise((resolve,reject)=>{
   const timer=setTimeout(()=>{this.pending.delete(id);const error=Error('Connection lost: RPC timeout; command result is uncertain');reject(error);this.lost(error);this.close();},4000);
   this.pending.set(id,{resolve,reject,timer});
   try{this.socket.send(JSON.stringify({...message,id}));}catch(error){clearTimeout(timer);this.pending.delete(id);reject(error);this.lost(error);this.close();}
  });
 }
 subscribe(device,interval){return this.request({type:'subscribe',device_id:device,interval_ms:interval});}
 async fetch(input,init={}){
  const path=new URL(String(input),'http://local').pathname;
  const headers={};new Headers(init.headers).forEach((value,name)=>{const canonical={'x-lab-lease':'X-Lab-Lease','idempotency-key':'Idempotency-Key','if-match':'If-Match'}[name];if(canonical)headers[canonical]=value;});
  const result=await this.request({method:init.method??'GET',path,headers,body:init.body?JSON.parse(String(init.body)):{}});
  return new Response(JSON.stringify(result.body),{status:result.status,headers:{'Content-Type':'application/json',...result.headers}});
 }
}
