import {panels,registerPanel,panelIds} from '../panels.js';
import {output} from './output';import {load} from './load';import {source} from './source';import {paths} from './switch';import {temperature} from './temperature';import {settings} from './settings';
import type {Device} from '../../generated/src/index';import type {PanelContext} from './context';
for(const definition of [output,load,source,paths,temperature,settings])registerPanel(definition);
export function renderDevicePanels(context:PanelContext,device:Device){for(const id of panelIds(device))panels.get(id)!.render(context,device);}
