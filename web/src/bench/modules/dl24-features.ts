// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import type {Dl24mSettings} from '../../../generated/src/index';
export const settingsFields:Record<string,{key:keyof Dl24mSettings;min:number;max:number;step:number;button?:string}>={
 dispBri:{key:'workingBrightness',min:0,max:9,step:1,button:'dispBriBtn'},
 stbyBri:{key:'standbyBrightness',min:0,max:9,step:1,button:'stbyBriBtn'},
 stbyTime:{key:'standbyTime',min:0,max:120,step:1,button:'stbyTimeBtn'},
 langSel:{key:'language',min:0,max:1,step:1,button:'langBtn'},
 ocp:{key:'overcurrentA',min:.05,max:2.5,step:.001},
 opp:{key:'overpowerW',min:1,max:15,step:.001},
 otext:{key:'batteryOvertemperatureC',min:40,max:100,step:1},
 otmos:{key:'overtemperatureC',min:40,max:100,step:1},
 cutv:{key:'cutoffVoltageV',min:0,max:25,step:.1},
 resolution:{key:'currentResolution',min:0,max:2,step:1,button:'resolutionBtn'},
 ovp:{key:'overvoltageV',min:3,max:25,step:.001,button:'ovpBtn'},
 fanTemp:{key:'fanTemperatureC',min:20,max:90,step:1,button:'fanTempBtn'}
};
// Every upstream action has either an adapter/local implementation or a reason.
export const features:Record<string,{handler?:string;reason?:string}>={
 connect:{handler:'connect'},pollMode:{handler:'local recording mode'},pollRate:{handler:'HTTP read interval'},theme:{handler:'local theme'},export:{handler:'session CSV'},gear:{handler:'settings dialog'},
 setBtn:{handler:'setComponentCurrent'},loadBtn:{handler:'setComponentOutput / stopDevice'},timeBtn:{handler:'setComponentDuration'},
 clearBtn:{handler:'session clear'},limBtn:{handler:'setDl24mSettings'},zeroData:{handler:'resetDl24mCounters'},
 settingsClose:{handler:'close dialog'},settingsBackdrop:{handler:'close dialog'},cvDismiss:{handler:'dismiss notice'},cvNever:{handler:'dismiss notice'},
 'mode-cc':{handler:'setDl24mSettings mode 0'},
 ...Object.fromEntries(Object.values(settingsFields).filter(s=>s.button).map(s=>[s.button!,{handler:'setDl24mSettings'}])),
 ...Object.fromEntries(['mode-cv','mode-cr','mode-cp'].map(id=>[id,{reason:'Active CV/CR/CP operation is not commissioned by the DL24M backend.'}])),
 ...Object.fromEntries(['calV','calI','calT','calVBtn','calIBtn','calTBtn','calCopy','calEnable'].map(id=>[id,{reason:'Calibration reads and writes are unavailable in the current backend.'}])),
 zeroNoLoad:{reason:'No-load zero has no supported backend operation.'},restoreDefaults:{reason:'Factory reset has no supported backend operation.'},
 ...Object.fromEntries(['snapLabel','snapBtn','snapClear'].map(id=>[id,{reason:'HID frames and snapshot comparison are unavailable over this backend.'}])),
 rRPM:{reason:'Fan RPM is not provided by the backend.'},rExt:{reason:'External probe data appears only when provided by the backend.'}
};
