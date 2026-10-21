// SPDX-License-Identifier: GPL-3.0-or-later
import type {Device} from '../../../generated/src/index';
import type {BenchModule} from './types';
import {dl24} from './dl24';
import {rd} from './rd';
const generic:BenchModule={id:'components',matches:()=>true,currentStep:0,voltageStep:0,canControl:()=>true};
export const benchModules:BenchModule[]=[dl24,rd];
export function moduleFor(device:Device){return benchModules.find(m=>m.matches(device))??generic;}
