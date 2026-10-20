import type {Configuration,CoreApi,Device,Operation} from '../../generated/src/index';
export interface PanelContext {
 config:Configuration;core:CoreApi;reading:HTMLElement;leased:boolean;
 params:(device:Device)=>{deviceId:string;xLabLease:string;idempotencyKey:string};
 input:(label:string,type?:string)=>HTMLInputElement;
 action:(label:string,fn:()=>Promise<unknown>,disabled?:boolean)=>HTMLButtonElement;
 wait:(operation:Operation)=>Promise<Operation>;key:()=>string;
}
export interface PanelDefinition {id:string;capability:string;render:(context:PanelContext,device:Device)=>void;}

export function numeric(input:HTMLInputElement){
 const value=input.valueAsNumber;
 if(!input.value.trim()||!Number.isFinite(value)||!input.checkValidity())throw Error('Enter a valid numeric value within the limits');
 return value;
}
