import test from 'node:test';
import assert from 'node:assert/strict';
import {SampleClock} from '../src/bench/sample-clock.js';
import {BenchSession} from '../src/bench/session.js';
const sample=(receivedMs,sequence)=>({deviceId:'synthetic-load',bootId:'boot-a',sequence,receivedMs,ageMs:100,quality:'valid',voltageV:10,currentA:.1,powerW:1,temperatureChannels:[{ageMs:20}],nativeReadings:[{id:'capacity',ageMs:50},{id:'fan',quality:'unavailable'}]});
test('queued telemetry retains transport age and cannot enter fresh integrals',()=>{
 const clock=new SampleClock();clock.start(0);const session=new BenchSession(3000);
 session.add(clock.adjust(sample(10000,1),20));session.add(clock.adjust(sample(11000,2),1020));const charge=session.chargeAh;
 const queued=clock.adjust(sample(12000,3),6020);
 assert.equal(queued.ageMs,4120);assert.equal(queued.temperatureChannels[0].ageMs,4040);assert.equal(queued.nativeReadings[0].ageMs,4070);assert.equal(queued.nativeReadings[1].ageMs,undefined);
 session.add(queued);assert.equal(session.chargeAh,charge);assert.equal(session.previous,null);assert.equal(session.points.at(-1).v,null);
 assert.equal(sample(12000,3).ageMs,100);
});
test('fresh data recovers after queued frames without reducing the established age bound',()=>{
 const clock=new SampleClock();clock.start(0);clock.adjust(sample(10000,1),20);
 assert.equal(clock.adjust(sample(12000,2),6020).ageMs,4120);
 assert.equal(clock.adjust(sample(17000,3),7020).ageMs,120);
});
test('new subscription resets clock mapping across device or ESP starts',()=>{
 const clock=new SampleClock();clock.start(0);clock.adjust(sample(90000,1),20);clock.start(10000);
 assert.equal(clock.adjust(sample(1000,2),10020).ageMs,120);
});
