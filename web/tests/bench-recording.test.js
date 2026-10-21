import {test} from 'node:test';
import assert from 'node:assert/strict';
import {BenchRecording} from '../src/bench/recording.js';
const sample=(deviceId,componentId,t,value,extra={})=>({deviceId,componentId,bootId:'boot-a',epoch:'1',quantity:componentId,unit:({voltage:'V',current:'A',power:'W',charge:'Ah'})[componentId]??'1',sequence:t,receivedMs:t,ageMs:0,quality:'valid',value,...extra});
test('all channels retain native counters and independent device integrals',()=>{
 const r=new BenchRecording();
 for(const t of [1000,2000])for(const id of ['a','b'])r.addBatch(id,[sample(id,'voltage',t,10),sample(id,'current',t,id==='a'?1:2),sample(id,'power',t,id==='a'?10:20),sample(id,'charge',t,99)],3000);
 assert.equal(r.integrals.get('a').chargeAh,1/3600);assert.equal(r.integrals.get('b').chargeAh,2/3600);
 assert.equal(r.channels.get('a/charge').latest.value,99);assert.match(r.csv(),/component_id.*epoch/);assert.match(r.csv(),/"native"/);assert.match(r.csv(),/"session"/);
});
test('duplicates, out-of-order samples, stale readings and epoch changes cannot bridge integration',()=>{
 const r=new BenchRecording();const add=(t,extra={})=>r.addBatch('a',['voltage','current','power'].map(c=>sample('a',c,t,1,extra)),3000);
 add(1000);add(2000);const count=r.rows.length;add(2000);assert.equal(r.rows.length,count);add(1500);assert.equal(r.rows.length,count);
 add(3000,{quality:'stale'});add(4000);add(5000,{epoch:'2'});assert.equal(r.integrals.get('a').chargeAh,1/3600);
 assert.ok(r.channels.get('a/voltage').points.some(p=>p.value===null));
 add(6000,{epoch:'2'});assert.equal(r.integrals.get('a').chargeAh,2/3600);
});
test('missing channels, gaps, negative values, shared row bounds and clear',()=>{
 const r=new BenchRecording(12,3);r.addBatch('a',[sample('a','voltage',1000,-1)],3000);
 r.addBatch('a',[sample('a','voltage',2000,undefined,{quality:'unavailable'})],3000);
 assert.equal(r.channels.get('a/voltage').points.at(-1).value,null);
 for(let t=3000;t<20000;t+=1000)r.addBatch('a',[sample('a','voltage',t,-2)],3000);
 assert.equal(r.rows.length,12);assert.equal(r.full,true);assert.ok(r.channels.get('a/voltage').points.length<=3);
 r.clear();assert.equal(r.rows.length,0);assert.equal(r.full,false);assert.equal(r.integrals.size,0);
});
test('a disconnected interval and missing power cannot create session charge',()=>{
 const r=new BenchRecording(),add=(t)=>r.addBatch('a',['voltage','current','power'].map(c=>sample('a',c,t,1)),3000);
 add(1000);r.break('a');add(2000);assert.equal(r.integrals.get('a').chargeAh,0);
 r.addBatch('a',[sample('a','voltage',2500,1),sample('a','current',2500,1)],3000);add(3000);assert.equal(r.integrals.get('a').chargeAh,0);
});
test('status CSV retains source metadata and formula-like IDs are escaped',()=>{
 const r=new BenchRecording();r.addStatus(sample('=device','voltage',1000,1),[{id:'regulation',value:'cv'},{id:'set-voltage',unit:'V',value:12}]);
 assert.match(r.csv(),/"status"/);assert.match(r.csv(),/"cv"/);assert.match(r.csv(),/"'=device"/);assert.match(r.csv(),/"set-voltage"/);
 r.addStatus(sample('=device','voltage',1000,1),[{id:'regulation',value:'cv'},{id:'set-voltage',unit:'V',value:12}]);assert.equal(r.rows.length,2);
});
