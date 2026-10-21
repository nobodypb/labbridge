// SPDX-License-Identifier: GPL-3.0-or-later
/** Node-local, multi-device recording. Visibility never changes acquisition. */
export class BenchRecording {
 constructor(maxRows=100000,maxPoints=900){
  this.maxRows=maxRows;this.maxPoints=maxPoints;
  /** @type {Map<string,any>} */ this.channels=new Map();
  /** @type {Map<string,any>} */ this.integrals=new Map();
  /** @type {Array<any>} */ this.rows=[];
  this.full=false;this.statusStamps=new Map();
 }
 clear(){this.statusStamps.clear();this.rows=[];this.integrals.clear();this.full=false;for(const c of this.channels.values()){c.points=[];c.stamp='';c.identity='';c.latestMs=-Infinity;}}
 break(deviceId){for(const [key,c] of this.channels)if(!deviceId||c.latest.deviceId===deviceId)this.gap(c);for(const [id,s] of this.integrals)if(!deviceId||id===deviceId)s.previous=null;}
 gap(c){if(c.points.length&&c.points.at(-1).value!==null){c.points.push({t:c.points.at(-1).t,value:null});this.trim(c);}}
 trim(c){if(c.points.length>this.maxPoints)c.points.splice(0,c.points.length-this.maxPoints);}
 append(row){if(this.full)return;this.rows.push(row);if(this.rows.length>=this.maxRows)this.full=true;}
 addBatch(deviceId,readings,maxAgeMs,record=true){
  let changed=false;
  for(const r of readings){
   const key=`${deviceId}/${r.componentId}`;
   let c=this.channels.get(key);
   if(!c){c={points:[],latestMs:-Infinity,stamp:'',identity:'',latest:r};this.channels.set(key,c);}
   const identity=JSON.stringify([r.bootId,r.epoch??'']),stamp=JSON.stringify([identity,r.sequence,r.receivedMs,r.quality,r.value]);
   if(identity===c.identity&&r.receivedMs<c.latestMs)continue;
   if(identity!==c.identity){this.gap(c);if(c.latest.bootId!==r.bootId)c.points=[];c.latestMs=-Infinity;}
   c.latest=r;c.maxAgeMs=maxAgeMs;c.receivedAt=performance.now();
   if(stamp===c.stamp)continue;
   const prior=c.points.at(-1);c.identity=identity;c.latestMs=r.receivedMs;c.stamp=stamp;
   if(!record||this.full){this.gap(c);continue;}
   const valid=r.quality==='valid'&&Number.isFinite(r.value)&&Number.isFinite(r.ageMs)&&r.ageMs>=0&&r.ageMs<=maxAgeMs&&Number.isFinite(r.receivedMs);
   if(prior&&r.receivedMs-prior.t>maxAgeMs)this.gap(c);
   c.points.push({t:r.receivedMs,value:valid?r.value:null});this.trim(c);
   this.append({...r,kind:'native',quality:valid?'valid':r.quality==='valid'?'unavailable':r.quality});changed=true;
  }
  if(!record||this.full){const integral=this.integrals.get(deviceId);if(integral)integral.previous=null;return;}
  // Prefer canonical output components. Ambiguous multi-meter quantities do not integrate.
  const find=(quantity,unit)=>{
   const candidates=readings.filter(r=>r.quantity===quantity&&r.unit===unit);
   return candidates.find(r=>r.componentId===quantity)??(candidates.length===1?candidates[0]:undefined);
  };
  const v=find('voltage','V'),i=find('current','A'),w=find('power','W');
  let s=this.integrals.get(deviceId);
  if(!s){s={chargeAh:0,energyWh:0,elapsedMs:0,previous:null,last:'',latestMs:-Infinity,identity:''};this.integrals.set(deviceId,s);}
  if(!v||!i||!w){s.previous=null;return;}
  const identity=JSON.stringify([v.bootId,v.epoch??'']),stamp=JSON.stringify([identity,v.sequence,v.receivedMs,v.quality,i.quality,w.quality]);
  if(stamp===s.last||identity===s.identity&&v.receivedMs<s.latestMs)return;
  s.last=stamp;if(identity!==s.identity){s.previous=null;s.latestMs=-Infinity;}s.identity=identity;s.latestMs=v.receivedMs;
  const valid=[v,i,w].every(r=>r.bootId===v.bootId&&(r.epoch??'')===(v.epoch??'')&&r.sequence===v.sequence&&r.receivedMs===v.receivedMs&&r.quality==='valid'&&Number.isFinite(r.value)&&Number.isFinite(r.ageMs)&&r.ageMs>=0&&r.ageMs<=maxAgeMs);
  const p=s.previous,dt=p?v.receivedMs-p.time:0;
  if(valid&&p&&dt>0&&dt<=maxAgeMs&&v.sequence!==p.sequence){
   s.chargeAh+=(p.current+i.value)/2*dt/3600000;s.energyWh+=(p.power+w.value)/2*dt/3600000;s.elapsedMs+=dt;
  }
  s.previous=valid?{time:v.receivedMs,sequence:v.sequence,current:i.value,power:w.value}:null;
  if(changed)for(const [componentId,quantity,unit,value] of [['session-charge','charge','Ah',s.chargeAh],['session-energy','energy','Wh',s.energyWh]])this.append({...v,componentId,quantity,unit,value,kind:'session',quality:valid?'valid':'unavailable'});
 }
 addStatus(sample,statuses){
  if(this.full)return;
  for(const status of statuses){
   const componentId=status.id,key=`${sample.deviceId}/${componentId}`;
   const stamp=JSON.stringify([sample.bootId,sample.sequence,sample.receivedMs,sample.quality,status.value]);
   if(this.statusStamps.get(key)===stamp)continue;this.statusStamps.set(key,stamp);
   this.append({...sample,componentId,quantity:'status',unit:status.unit??'1',value:status.value,kind:'status'});
  }
 }
 csv(){
  const keys=['deviceId','componentId','bootId','epoch','sequence','receivedMs','ageMs','quality','quantity','unit','kind','value'];
  const header='device_id,component_id,boot_id,epoch,sequence,received_ms,age_ms,quality,quantity,unit,kind,value';
  const escape=v=>{if(v===undefined||v===null)return '';let s=String(v);if(typeof v==='string'&&/^[=+@\-\t\r]/.test(s))s="'"+s;return '"'+s.replaceAll('"','""')+'"';};
  return header+'\r\n'+this.rows.map(r=>keys.map(k=>escape(r[k])).join(',')).join('\r\n')+'\r\n';
 }
}
