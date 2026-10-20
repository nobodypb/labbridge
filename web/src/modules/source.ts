import {ComponentsApi} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const source:PanelDefinition={id:'source',capability:'source.cc_cv',render(ctx,d){
 const api=new ComponentsApi(ctx.config),v=ctx.input('Source voltage (V)','number'),a=ctx.input('Source current (A)','number');v.min=a.min='0';v.max=String(d.limits?.voltageV??0);a.max=String(d.limits?.currentA??0);v.step=a.step='.01';
 ctx.action('Set voltage',async()=>ctx.wait(await api.setComponentVoltage({...ctx.params(d),componentId:"source",voltageRequest:{voltageV:Number(v.value)}})),!ctx.leased);
 ctx.action('Set current',async()=>ctx.wait(await api.setComponentCurrent({...ctx.params(d),componentId:"source",currentRequest:{currentA:Number(a.value)}})),!ctx.leased);
}};
