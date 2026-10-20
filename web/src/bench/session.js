// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
/** Browser-only recording; invalid intervals never contribute charge or energy. */
export class BenchSession {
 /** @type {Array<Record<string, any>>} */ rows=[];
 /** @type {Array<Record<string, any>>} */ points=[];
 chargeAh=0;energyWh=0;elapsedMs=0;full=false;
 constructor(maxGapMs, maxRows=100000, maxPoints=900) {
  this.maxGapMs=maxGapMs;this.maxRows=maxRows;this.maxPoints=maxPoints;this.clear();
 }
 clear(){this.rows=[];this.points=[];this.chargeAh=0;this.energyWh=0;this.elapsedMs=0;this.full=false;this.identity=null;this.previous=null;this.lastStamp=null;this.latestMs=-Infinity;}
 break(){this.previous=null;this.points.push({t:null,v:null,i:null,w:null});this.trim();}
 trim(){if(this.points.length>this.maxPoints)this.points.splice(0,this.points.length-this.maxPoints);}
 add(m){
  if(this.full)return false;
  const identity=JSON.stringify([m.deviceId,m.bootId]);
  if(this.identity!==null&&identity!==this.identity)throw Error('Start a new session after device or boot changes');
  this.identity=identity;
  if(m.quality==='valid'&&m.receivedMs<this.latestMs)return false;
  if(Number.isFinite(m.receivedMs))this.latestMs=Math.max(this.latestMs,m.receivedMs);
  const stamp=JSON.stringify([m.sequence,m.receivedMs,m.quality]);
  if(stamp===this.lastStamp)return false;
  this.lastStamp=stamp;
  const valid=m.quality==='valid'&&Number.isFinite(m.ageMs)&&m.ageMs>=0&&m.ageMs<=this.maxGapMs&&
   Number.isFinite(m.receivedMs)&&[m.voltageV,m.currentA,m.powerW].every(Number.isFinite);
  const p=this.previous,dt=p?m.receivedMs-p.receivedMs:0;
  if(valid&&p&&dt>0&&dt<=this.maxGapMs&&m.sequence!==p.sequence){
   this.chargeAh+=(p.currentA+m.currentA)/2*dt/3600000;
   this.energyWh+=(p.powerW+m.powerW)/2*dt/3600000;
   this.elapsedMs+=dt;
  }else if(p&&dt>this.maxGapMs)this.break();
  this.previous=valid?m:null;
  this.points.push({t:this.latestMs,v:valid?m.voltageV:null,i:valid?m.currentA:null,w:valid?m.powerW:null});this.trim();
  this.rows.push({...m,quality:valid?'valid':m.quality==='valid'?'unavailable':m.quality,capacityAh:this.chargeAh,energyWh:this.energyWh});
  if(this.rows.length>=this.maxRows)this.full=true;
  return true;
 }
 csv(){
  const keys=['deviceId','bootId','sequence','receivedMs','ageMs','quality','voltageV','currentA','powerW','internalTemperatureC','externalTemperatureC','capacityAh','energyWh'];
  const head='device_id,boot_id,sequence,received_ms,age_ms,quality,voltage_V,current_A,power_W,internal_temperature_C,external_temperature_C,session_capacity_Ah,session_energy_Wh';
  const escape=v=>{if(v===undefined||v===null)return '';let s=String(v);if(typeof v==='string'&&/^[=+@\-\t\r]/.test(s))s="'"+s;return '"'+s.replaceAll('"','""')+'"';};
  return head+'\r\n'+this.rows.map(r=>keys.map(k=>escape(r[k])).join(',')).join('\r\n')+'\r\n';
 }
}
