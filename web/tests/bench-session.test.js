import {test} from 'node:test';
import assert from 'node:assert/strict';
import {BenchSession} from '../src/bench/session.js';
const sample=(time,extra={})=>({deviceId:'load-a',bootId:'boot-a',sequence:time,receivedMs:time,ageMs:0,quality:'valid',voltageV:10,currentA:1,powerW:10,...extra});
test('session integrates fresh distinct samples, exports units and preserves gaps',()=>{
 const s=new BenchSession(3000);
 s.add(sample(1000));s.add(sample(2000));s.add(sample(2000));
 assert.equal(s.rows.length,2);assert.equal(s.chargeAh,1/3600);assert.equal(s.energyWh,10/3600);
 s.add(sample(3000,{quality:'stale'}));s.add(sample(4000));
 assert.equal(s.chargeAh,1/3600);assert.equal(s.points.at(-2).v,null);
 s.add(sample(8000));assert.equal(s.chargeAh,1/3600);
 assert.match(s.csv(),/device_id,boot_id,sequence,received_ms,age_ms,quality/);
 assert.match(s.csv(),/voltage_V,current_A,power_W/);
});
test('identity and boot changes cannot mix sessions; duplicate timestamps do not integrate',()=>{
 const s=new BenchSession(3000);s.add(sample(1000));s.add(sample(1000,{sequence:1001}));
 assert.equal(s.chargeAh,0);
 assert.throws(()=>s.add(sample(2000,{bootId:'boot-b'})),/session/i);
 assert.throws(()=>s.add(sample(2000,{deviceId:'load-b'})),/session/i);
});
test('session bounds memory, pauses visibly and clear resets only local counters',()=>{
 const s=new BenchSession(3000,3,2);[1000,2000,3000,4000].forEach(t=>s.add(sample(t)));
 assert.equal(s.rows.length,3);assert.equal(s.points.length,2);assert.equal(s.full,true);
 s.clear();assert.equal(s.rows.length,0);assert.equal(s.energyWh,0);assert.equal(s.full,false);
});
test('missing values are not zeros and a discontinuity breaks integration',()=>{
 const s=new BenchSession(3000);s.add(sample(1000));s.break();s.add(sample(2000));
 assert.equal(s.energyWh,0);s.add(sample(3000,{voltageV:undefined}));
 assert.equal(s.points.at(-1).v,null);assert.equal(s.energyWh,0);
});
test('late out-of-order samples cannot be integrated twice',()=>{
 const s=new BenchSession(3000);s.add(sample(1000));s.add(sample(2000));
 s.add(sample(1500));s.add(sample(3000));
 assert.equal(s.chargeAh,2/3600);assert.equal(s.rows.length,3);
});
test('full-buffer and paused recording gaps stay bounded',()=>{
 const s=new BenchSession(3000,2,3);s.add(sample(1000));s.add(sample(2000));
 for(let i=0;i<20;i++)s.break();
 assert.ok(s.points.length<=3);assert.equal(s.rows.length,2);
});

test('CSV preserves native counters, provenance and quality separately from session integrals',()=>{
 const s=new BenchSession(3000);s.add(sample(1000,{nativeReadings:[{id:'capacity',value:123,unit:'raw',origin:'dl24m.ble.14',quality:'valid',receivedMs:1000,ageMs:0}]}));
 assert.match(s.csv(),/native_readings_json/);assert.match(s.csv(),/dl24m.ble.14/);assert.match(s.csv(),/session_capacity_Ah/);
});
