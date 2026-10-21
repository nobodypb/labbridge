// SPDX-License-Identifier: GPL-3.0-or-later
import {Dl24mApi} from '../../../generated/src/index';
import type {BenchModule} from './types';
import type {Dl24mSettings} from '../../../generated/src/index';
import {mountDl24} from './dl24-ui';
export const dl24:BenchModule={
 id:'dl24m',mount:mountDl24,matches:d=>d.capabilities.includes('dl24m.settings'),currentStep:.001,voltageStep:.001,
 canControl:settings=>(settings as Dl24mSettings).mode===0,
 readSettings:(config,deviceId)=>new Dl24mApi(config).getDl24mSettings({deviceId}),
 writeSettings:(config,params,settings)=>new Dl24mApi(config).setDl24mSettings({...params,dl24mSettings:settings as Dl24mSettings}),
 resetCounters:(config,params)=>new Dl24mApi(config).resetDl24mCounters({...params,stopRequest:{reason:'Bench explicit counter reset'}})
};
