// SPDX-License-Identifier: GPL-3.0-or-later
// Keep a conservative clock bound from the request that caused the first sample.
// Arrival time must never turn already queued telemetry into a fresh reading.
export class SampleClock {
 reference=0;
 /** @type {number|undefined} */ offset;
 start(now){this.reference=now;this.offset=undefined;}
 adjust(sample,now){
  if(!Number.isFinite(sample.receivedMs)||!Number.isFinite(sample.ageMs)||sample.ageMs<0)return {...sample,ageMs:Infinity};
  if(this.offset===undefined)this.offset=this.reference-(sample.receivedMs+sample.ageMs);
  const delay=Math.max(0,now-sample.receivedMs-this.offset-sample.ageMs);
  const age=value=>value.ageMs===undefined?value:{...value,ageMs:value.ageMs+delay};
  return {...sample,ageMs:sample.ageMs+delay,temperatureChannels:sample.temperatureChannels.map(age),nativeReadings:sample.nativeReadings?.map(age)};
 }
}
