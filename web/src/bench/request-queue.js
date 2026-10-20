// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
/** One in-flight request on the ESP's shared socket budget; no retries. */
export class RequestQueue {
 /** @type {Array<{task:()=>Promise<any>,resolve:(value:any)=>void,reject:(error:any)=>void,priority:number}>} */
 pending=[];
 active=false;
 /** @template T @param {()=>Promise<T>} task @param {number} priority @returns {Promise<T>} */
 run(task,priority=3){
  return new Promise((resolve,reject)=>{this.pending.push({task,priority,resolve,reject});this.pending.sort((a,b)=>a.priority-b.priority);this.drain();});
 }
 cancelPending(error,predicate=job=>true){const jobs=this.pending.splice(0);for(const job of jobs){if(predicate(job))job.reject(error);else this.pending.push(job);}}
 drain(){
  if(this.active||!this.pending.length)return;
  const job=this.pending.shift();this.active=true;
  void (async()=>{try{job.resolve(await job.task());}catch(error){job.reject(error);}finally{this.active=false;this.drain();}})();
 }
}
