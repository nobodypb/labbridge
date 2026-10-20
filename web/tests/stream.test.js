import {test} from 'node:test';
import assert from 'node:assert/strict';
import {RpcStream} from '../src/bench/stream.js';
class Socket {
 static sockets=[];readyState=0;sent=[];
 constructor(){Socket.sockets.push(this);queueMicrotask(()=>{this.readyState=1;this.onopen?.();});}
 send(s){this.sent.push(JSON.parse(s));}
 close(){this.readyState=3;this.onclose?.();}
 receive(o){this.onmessage?.({data:JSON.stringify(o)});}
}
const tick=()=>new Promise(r=>setImmediate(r));
test('authenticates before RPC, correlates USB envelopes and reports gaps',async()=>{
 const stream=new RpcStream('ws://node/api/v1/ws',Socket);let gaps=0,samples=[];
 stream.onGap=()=>gaps++;stream.onSnapshot=e=>samples.push(e);
 const opening=stream.open('secret');await tick();const socket=Socket.sockets.at(-1);
 assert.deepEqual(socket.sent,[{type:'auth',token:'secret'}]);
 socket.receive({type:'authenticated',boot_id:'boot-a'});await opening;
 const result=stream.fetch('/api/v1/node',{method:'GET',headers:{Authorization:'Bearer ignored'}});await tick();
 const request=socket.sent.at(-1);assert.equal(request.method,'GET');assert.equal(request.path,'/api/v1/node');
 socket.receive({id:request.id,status:200,headers:{},body:{boot_id:'boot-a'}});
 assert.equal((await (await result).json()).boot_id,'boot-a');
 socket.receive({type:'snapshot',boot_id:'boot-a',sequence:1,gap:false,body:{}});
 socket.receive({type:'snapshot',boot_id:'boot-a',sequence:3,gap:false,body:{}});
 assert.equal(gaps,1);assert.equal(samples.length,2);stream.close();
});
test('connection loss rejects an uncertain write and never replays it',async()=>{
 const stream=new RpcStream('ws://node/api/v1/ws',Socket);
 const opening=stream.open('secret');await tick();let socket=Socket.sockets.at(-1);socket.receive({type:'authenticated',boot_id:'a'});await opening;
 const writing=stream.fetch('/api/v1/devices/x/output',{method:'PUT',body:'{"enabled":true}'});
 const rejected=assert.rejects(writing,/Connection lost/);await tick();socket.close();await rejected;
 const reconnect=stream.open('secret');await tick();socket=Socket.sockets.at(-1);socket.receive({type:'authenticated',boot_id:'b'});await reconnect;
 assert.deepEqual(socket.sent,[{type:'auth',token:'secret'}]);stream.close();
});
test('rejects unauthenticated access and never exposes auth in a URL',async()=>{
 const stream=new RpcStream('ws://node/api/v1/ws',Socket);
 await assert.rejects(stream.fetch('/api/v1/node',{}),/authenticated/);
 const opening=stream.open('bad');const rejected=assert.rejects(opening,/Token/);await tick();Socket.sockets.at(-1).receive({status:401,body:{detail:'Token rejected'}});await rejected;stream.close();
});
test('bounds pending RPCs and rejects all on close without retry',async()=>{
 const stream=new RpcStream('ws://node/api/v1/ws',Socket);
 const opening=stream.open('secret');await tick();const socket=Socket.sockets.at(-1);socket.receive({type:'authenticated',boot_id:'a'});await opening;
 const requests=Array.from({length:8},()=>stream.fetch('/api/v1/node',{}));
 const rejections=requests.map(p=>assert.rejects(p,/Connection changed/));
 await assert.rejects(stream.fetch('/api/v1/node',{}),/buffer full/);stream.close();await Promise.all(rejections);
 assert.equal(socket.sent.length,9);
});
test('restart rejects the session and reordered or duplicate events are ignored',async()=>{
 const stream=new RpcStream('ws://node/api/v1/ws',Socket);let events=0,loss='';stream.onSnapshot=()=>events++;stream.onLoss=e=>loss=e.message;
 const opening=stream.open('secret');await tick();const socket=Socket.sockets.at(-1);socket.receive({type:'authenticated',boot_id:'a'});await opening;
 for(const seq of [1,1,0])socket.receive({type:'snapshot',boot_id:'a',sequence:seq,gap:false,body:{}});
 assert.equal(events,1);socket.receive({type:'snapshot',boot_id:'b',sequence:2,body:{}});
 assert.match(loss,/restarted/);assert.equal(stream.ready,false);stream.close();
});
