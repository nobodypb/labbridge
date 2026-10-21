import {test} from 'node:test';
import assert from 'node:assert/strict';
import {CanvasChart} from '../src/bench/chart.js';
globalThis.devicePixelRatio=1;
test('same-unit channels share one scale while each uses its actual sample times',()=>{
 const labels=[],traces=[];let path=[];
 const ctx={setTransform(){},clearRect(){},fillText(t){labels.push(t);},beginPath(){path=[];},moveTo(x,y){path.push(['move',x,y]);},lineTo(x,y){path.push(['line',x,y]);},stroke(){traces.push({color:this.strokeStyle,path});}};
 const canvas={parentElement:{clientWidth:600,clientHeight:400},style:{},getContext:()=>ctx};
 const chart=new CanvasChart(canvas);chart.setSeries([
  {unit:'V',color:'red',points:[{t:1000,value:1},{t:2000,value:1}]},
  {unit:'V',color:'blue',points:[{t:1500,value:10},{t:2000,value:10}]}
 ]);
 assert.equal(labels.filter(t=>t==='V').length,1);
 const a=traces.find(t=>t.color==='red').path,b=traces.find(t=>t.color==='blue').path;
 assert.ok(a[0][2]>b[0][2],'lower voltage must be below higher voltage on the shared scale');
 assert.ok(b[0][1]>a[0][1],'later samples must start later on the common time axis');
 chart.setSeries([{unit:'V',color:'red',points:[{t:1000,value:1},{t:1500,value:null},{t:2000,value:1}]}]);
 assert.equal(traces.at(-1).path.filter(p=>p[0]==='move').length,2,'a data gap must start a separate trace');
});
test('canvas follows its viewport height and pixel ratio when resized',()=>{
 const ctx={setTransform(){},clearRect(){},fillText(){},beginPath(){},moveTo(){},lineTo(){},stroke(){}};
 const parentElement={clientWidth:800,clientHeight:620},canvas={parentElement,style:{},getContext:()=>ctx};
 const chart=new CanvasChart(canvas);
 try{
  globalThis.devicePixelRatio=2;chart.fit();assert.equal(canvas.width,1600);assert.equal(canvas.height,1240);assert.equal(canvas.style.height,'620px');
  parentElement.clientHeight=440;parentElement.clientWidth=700;chart.fit();assert.equal(canvas.height,880);assert.equal(canvas.width,1400);
 }finally{globalThis.devicePixelRatio=1;}
});
