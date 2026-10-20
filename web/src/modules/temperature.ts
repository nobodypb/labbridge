import {Ds18b20Api} from '../../generated/src/index';
import type {PanelDefinition} from './context';
export const temperature:PanelDefinition={id:'temperature',capability:'temperature.external',render(ctx,d){
 const api=new Ds18b20Api(ctx.config);
 ctx.action('Read temperature channels',async()=>{ctx.reading.textContent=JSON.stringify(await api.getTemperatureChannels({deviceId:d.id}),null,2);});
}};
