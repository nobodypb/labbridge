import {Rd60xxApi} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const source:PanelDefinition={id:'source',capability:'source.cc_cv',render(ctx,d){
 const api=new Rd60xxApi(ctx.config),v=ctx.input('Source voltage (V)','number'),a=ctx.input('Source current (A)','number');v.min=a.min='0';v.max=String(d.limits.voltageV);a.max=String(d.limits.currentA);v.step=a.step='.01';
 ctx.action('Set voltage',async()=>ctx.wait(await api.setSourceVoltage({...ctx.params(d),voltageRequest:{voltageV:Number(v.value)}})),!ctx.leased);
 ctx.action('Set current',async()=>ctx.wait(await api.setSourceCurrent({...ctx.params(d),currentRequest:{currentA:Number(a.value)}})),!ctx.leased);
}};
