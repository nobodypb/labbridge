import {ComponentsApi} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const load:PanelDefinition={id:'load',capability:'load.cc',render(ctx,d){
 const api=new ComponentsApi(ctx.config),a=ctx.input('Load current (A)','number');a.min='0';a.max=String(d.limits?.currentA??0);a.step='.001';
 ctx.action('Set load current',async()=>ctx.wait(await api.setComponentCurrent({...ctx.params(d),componentId:"sink",currentRequest:{currentA:Number(a.value)}})),!ctx.leased);
}};
