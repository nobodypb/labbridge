// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 LabBridge contributors
import type {Dl24mSettings} from '../../generated/src/index';
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
export const features:Record<string,{handler?:string;reason?:string;capability?:string}>={
 connect:{handler:'connect'},pollMode:{handler:'local recording mode'},pollRate:{handler:'stream measurement interval'},theme:{handler:'local theme'},export:{handler:'session CSV'},gear:{handler:'settings dialog'},
 setBtn:{handler:'setComponentCurrent / setDl24mTarget'},loadBtn:{handler:'setComponentOutput / stopDevice'},timeBtn:{handler:'setComponentDuration'},
 clearBtn:{handler:'session clear'},limBtn:{handler:'setDl24mSettings'},zeroData:{handler:'resetDl24mCounters'},
 settingsClose:{handler:'close dialog'},settingsBackdrop:{handler:'close dialog'},cvDismiss:{handler:'dismiss notice'},cvNever:{handler:'dismiss notice'},
 'mode-cc':{handler:'setDl24mMode'},
 ...Object.fromEntries(Object.values(settingsFields).filter(s=>s.button).map(s=>[s.button!,{handler:'setDl24mSettings'}])),
 ...Object.fromEntries(['cv','cr','cp'].map(mode=>['mode-'+mode,{capability:mode,reason:'Connect to discover active mode support.'}])),
 ...Object.fromEntries(['calV','calI','calT','calVBtn','calIBtn','calTBtn','calCopy','calEnable'].map(id=>[id,{capability:'calibration',reason:'Connect to discover calibration support.'}])),
 zeroNoLoad:{capability:'zero',reason:'No-load zero has no supported backend operation.'},restoreDefaults:{capability:'factory-reset',reason:'Factory reset has no supported backend operation.'},
 snapLabel:{capability:'diagnostics',handler:'local snapshot label'},snapBtn:{capability:'diagnostics',handler:'getDl24mDiagnostics'},snapClear:{capability:'diagnostics',handler:'clear browser snapshots'},
 rRPM:{capability:'fan-rpm',reason:'Fan RPM is not provided by the backend.'},rExt:{capability:'external-temperature',reason:'External probe data appears only when provided by the backend.'}
};
