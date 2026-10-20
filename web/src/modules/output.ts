import type {PanelDefinition} from './context';
import {canEnable} from '../panels.js';
export const output:PanelDefinition={id:'output',capability:'output',render(ctx,d){
 if(!d.components?.some(c=>c.kind==='source'||c.kind==='sink')&&!d.capabilities.includes('switch.path'))ctx.action('Output ON',async()=>ctx.wait(await ctx.core.setOutput({...ctx.params(d),outputRequest:{enabled:true}})),!canEnable(d,ctx.leased));
 ctx.action('STOP',async()=>ctx.wait(await ctx.core.stopDevice({deviceId:d.id,idempotencyKey:ctx.key(),stopRequest:{reason:'UI stop'}})));
}};
