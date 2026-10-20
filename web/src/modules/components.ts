import {numeric} from './context';
import {ComponentsApi} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const components:PanelDefinition={id:'components',capability:'components',render(ctx,d){
 const api=new ComponentsApi(ctx.config);
 for(const c of d.components??[]){
  if(c.kind==='sensor')continue;
  const request=()=>({...ctx.params(d),componentId:c.id});
  const disabled=!ctx.leased||d.state!=='ready'||d.output==='unknown';
  if(c.actions.includes('current')){
   const i=ctx.input(`${c.id} current (A)`,'number');i.min='0';i.step='any';if(d.limits)i.max=String(d.limits.currentA);
   ctx.action(`${c.id}: set current`,async()=>ctx.wait(await api.setComponentCurrent({...request(),currentRequest:{currentA:numeric(i)}})),disabled);
  }
  if(c.actions.includes('voltage')){
   const v=ctx.input(`${c.id} voltage (V)`,'number');v.min='0';v.step='any';if(d.limits)v.max=String(d.limits.voltageV);
   ctx.action(`${c.id}: set voltage`,async()=>ctx.wait(await api.setComponentVoltage({...request(),voltageRequest:{voltageV:numeric(v)}})),disabled);
  }
  if(c.actions.includes('output')){
   const button=ctx.action(`${c.id}: ON`,async()=>{
    const state=await api.getActuatorState({deviceId:d.id,componentId:c.id});
    if(state.state!=='ready'||state.output!=='off')throw Error('Actuator requires ready and confirmed OFF');
    return ctx.wait(await api.setComponentOutput({...request(),outputRequest:{enabled:true}}));
   },true);
   void api.getActuatorState({deviceId:d.id,componentId:c.id}).then(state=>{button.disabled=!ctx.leased||state.state!=='ready'||state.output!=='off';}).catch(()=>{button.disabled=true;});
  }
  ctx.action(`${c.id}: STOP`,async()=>ctx.wait(await api.stopComponent({deviceId:d.id,componentId:c.id,idempotencyKey:ctx.key(),stopRequest:{reason:'Component UI stop'}})));
 }
 ctx.action(`Read sensors: ${d.id}`,async()=>{ctx.reading.textContent=JSON.stringify(await api.getSensorReadings({deviceId:d.id}),null,2);});
}};
