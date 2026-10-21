// SPDX-License-Identifier: GPL-3.0-or-later
import type {BenchModule} from './types';
export const rd:BenchModule={
 id:'rd60xx',matches:d=>d.driver==='rd60xx',currentStep:.01,voltageStep:.01,canControl:()=>true,
 status:m=>[
  {id:'regulation-mode',label:'Regulation',value:m.regulationMode?.toUpperCase()??'unknown'},
  {id:'protection-code',label:'Protection code',value:m.protectionCode===undefined?'unknown':String(m.protectionCode)}
 ]
};
