import {Dl24mApi} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const settings:PanelDefinition={id:'settings',capability:'dl24m.settings',render(ctx,d){
 const api=new Dl24mApi(ctx.config);
 ctx.action('Read load settings',async()=>{ctx.reading.textContent=JSON.stringify(await api.getDl24mSettings({deviceId:d.id}),null,2);});
 const cutoff=ctx.input('Discharge cutoff (V)','number');cutoff.min='0';cutoff.step='.1';
 ctx.action('Set cutoff',async()=>ctx.wait(await api.setDl24mSettings({...ctx.params(d),dl24mSettings:{cutoffVoltageV:Number(cutoff.value)}})),!ctx.leased);
 ctx.action('Reset counters',async()=>ctx.wait(await api.resetDl24mCounters({...ctx.params(d),stopRequest:{reason:'UI counter reset'}})),!ctx.leased);
}};
