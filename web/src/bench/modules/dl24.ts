// SPDX-License-Identifier: GPL-3.0-or-later
import {Dl24mApi,Dl24mSettingsFromJSON,ComponentsApi} from '../../../generated/src/index';
import type {BenchModule} from './types';
import type {Dl24mSettings} from '../../../generated/src/index';
import {mountDl24} from './dl24-ui';
export const dl24:BenchModule={
 id:'dl24m',mount:mountDl24,matches:d=>d.capabilities.includes('dl24m.settings'),currentStep:.001,voltageStep:.001,
 canControl:(settings,device)=>(settings as Dl24mSettings).mode===0||!!device?.features?.some(f=>f.id===(['cc','cv','cr','cp'][(settings as Dl24mSettings).mode??-1])&&f.status==='supported'),
 parseSettings:raw=>Dl24mSettingsFromJSON(raw),
 target:(device,settings)=>{
  const mode=(settings as Dl24mSettings).mode,limits=device.limits;
  return mode===1?{min:.5,max:limits?.voltageV??0,step:.01,unit:'V',live:false}:mode===2?{min:.1,max:240,step:.1,unit:'Ω',live:false}:mode===3?{min:.1,max:limits?.powerW??0,step:.1,unit:'W',live:false}:{min:0,max:limits?.currentA??0,step:.001,unit:'A',live:true};
 },
 writeTarget:(config,params,settings,value)=>{
  const mode=(settings as Dl24mSettings).mode;
  if(mode===0)return new ComponentsApi(config).setComponentCurrent({...params,currentRequest:{currentA:value}});
  if(mode!==1&&mode!==2&&mode!==3)throw Error('Supported mode required');
  return new Dl24mApi(config).setDl24mTarget({...params,dl24mTargetRequest:{mode:mode===1?'cv':mode===2?'cr':'cp',value,unit:mode===1?'V':mode===2?'Ohm':'W'}});
 },
 readSettings:(config,deviceId)=>new Dl24mApi(config).getDl24mSettings({deviceId}),
 writeSettings:(config,params,settings)=>new Dl24mApi(config).setDl24mSettings({...params,dl24mSettings:settings as Dl24mSettings}),
 resetCounters:(config,params)=>new Dl24mApi(config).resetDl24mCounters({...params,stopRequest:{reason:'Bench explicit counter reset'}})
};
