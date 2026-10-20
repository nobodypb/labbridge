import {GpioInterlockApi} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const paths:PanelDefinition={id:'interlock',capability:'switch.path',render(ctx,d){
 const api=new GpioInterlockApi(ctx.config);
 ctx.action('Path: isolated',async()=>ctx.wait(await ctx.core.stopDevice({deviceId:d.id,idempotencyKey:ctx.key(),stopRequest:{reason:'UI isolate'}})));
 for(const path of ['charge','discharge'] as const)ctx.action(`Path: ${path}`,async()=>ctx.wait(await api.setSwitchPath({...ctx.params(d),switchPathRequest:{path}})),!ctx.leased);
}};
