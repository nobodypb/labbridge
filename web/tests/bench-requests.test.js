import {test} from 'node:test';
import assert from 'node:assert/strict';
import {RequestQueue} from '../src/bench/request-queue.js';
test('one request in flight, with STOP and heartbeat ahead of queued reads',async()=>{
 const queue=new RequestQueue(),order=[];let release;
 const first=queue.run(()=>new Promise(resolve=>{order.push('active');release=resolve;}),3);
 const read=queue.run(async()=>order.push('read'),3);
 const stop=queue.run(async()=>order.push('stop'),0);
 const heartbeat=queue.run(async()=>order.push('heartbeat'),1);
 release();await Promise.all([first,read,stop,heartbeat]);assert.deepEqual(order,['active','stop','heartbeat','read']);
});
test('connection loss cancels queued mutations without running or replaying them',async()=>{
 const queue=new RequestQueue();let release,writes=0;
 const active=queue.run(()=>new Promise(resolve=>{release=resolve;}),3);
 const mutation=queue.run(async()=>writes++,2);
 const rejected=assert.rejects(mutation,/lost/);
 queue.cancelPending(Error('Connection lost'));release();await active;await rejected;assert.equal(writes,0);
 await queue.run(async()=>42,3);assert.equal(writes,0);
});
