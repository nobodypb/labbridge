// SPDX-License-Identifier: GPL-3.0-or-later
import type {Configuration,Device,Measurements} from '../../../generated/src/index';
import type {BenchAdapter} from '../adapter';
export type BenchSettings = object;
export interface BenchModuleContext {
 api:BenchAdapter;
 input:(id:string)=>HTMLInputElement;
 element:(id:string)=>HTMLElement;
 bind:(id:string,fn:()=>Promise<unknown>,priority?:boolean)=>void;
 number:(id:string,min:number,max:number,step:number)=>number;
}
export interface BenchModuleView {
 render:(pending:boolean)=>void;
 saveDraft:(draft:Record<string,string>)=>void;
 restoreDraft:(draft:Record<string,string>)=>void;
}
export interface BenchModule {
 id:string;
 mount?:(context:BenchModuleContext)=>BenchModuleView;
 matches:(device:Device)=>boolean;
 currentStep:number;
 voltageStep:number;
 canControl:(settings:BenchSettings,device?:Device)=>boolean;
 parseSettings?:(raw:unknown)=>BenchSettings;
 target?:(device:Device,settings:BenchSettings)=>{min:number;max:number;step:number;unit:string;live:boolean};
 writeTarget?:(config:Configuration,params:{deviceId:string;componentId:string;xLabLease:string;idempotencyKey:string},settings:BenchSettings,value:number)=>Promise<import('../../../generated/src/index').Operation>;
 readSettings?:(config:Configuration,deviceId:string)=>Promise<BenchSettings>;
 writeSettings?:(config:Configuration,params:{deviceId:string;xLabLease:string;idempotencyKey:string},settings:BenchSettings)=>Promise<import('../../../generated/src/index').Operation>;
 resetCounters?:(config:Configuration,params:{deviceId:string;xLabLease:string;idempotencyKey:string})=>Promise<import('../../../generated/src/index').Operation>;
 status?:(measurement:Measurements)=>Array<{id:string;label:string;unit?:string;value:string|number}>;
}
