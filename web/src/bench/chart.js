// SPDX-License-Identifier: GPL-3.0-or-later
// Derived from DL24 Bench, Copyright © 2026 project contributors.
/** Independent channel samples on a common node clock, with one scale per unit. */
export class CanvasChart {
 constructor(canvas,opts={}){this.c=canvas;this.css=opts.css||(v=>v);this.series=[];}
 setSeries(series){this.series=series;this.fit();}
 fit(){
  const c=this.c,dpr=devicePixelRatio||1,units=new Set(this.series.map(s=>s.unit));
  const width=Math.max(c.parentElement.clientWidth,190+units.size*48),height=Math.max(1,c.parentElement.clientHeight);
  if(c.width!==Math.round(width*dpr)||c.height!==Math.round(height*dpr)){
   c.style.width=width+'px';c.style.height=height+'px';c.width=Math.round(width*dpr);c.height=Math.round(height*dpr);
   c.getContext('2d').setTransform(dpr,0,0,dpr,0,0);
  }
  this.redraw();
 }
 redraw(){
  const c=this.c,ctx=c.getContext('2d');if(!ctx)return;
  const dpr=devicePixelRatio||1,W=c.width/dpr,H=c.height/dpr;ctx.clearRect(0,0,W,H);
  const units=[...new Set(this.series.map(s=>s.unit))],ranges=new Map();
  let x0=Infinity,x1=-Infinity;
  for(const unit of units){
   let lo=Infinity,hi=-Infinity;
   for(const s of this.series.filter(s=>s.unit===unit))for(const p of s.points){if(Number.isFinite(p.t)){x0=Math.min(x0,p.t);x1=Math.max(x1,p.t);}if(Number.isFinite(p.value)){lo=Math.min(lo,p.value);hi=Math.max(hi,p.value);}}
   if(!Number.isFinite(lo)){lo=0;hi=1;}
   const margin=Math.max((hi-lo)*.06,Math.abs(hi)*.02,.01),nonnegative=lo>=0;
   ranges.set(unit,[nonnegative?Math.max(0,lo-margin):lo-margin,hi+margin]);
  }
  const left=10+Math.max(1,units.length)*48,top=30,bottom=H-28,pw=W-left-12,ph=bottom-top;
  ctx.font='10px ui-monospace,monospace';ctx.lineWidth=1;
  units.forEach((unit,j)=>{ctx.fillStyle=this.css('--ink');ctx.textAlign='right';ctx.fillText(unit,48*(j+1),15);});
  for(let j=0;j<=4;j++){
   const y=top+ph*j/4;ctx.strokeStyle=this.css('--grid');ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(W-12,y);ctx.stroke();
   units.forEach((unit,k)=>{const [lo,hi]=ranges.get(unit),v=hi-(hi-lo)*j/4;ctx.fillStyle=this.css('--dim');ctx.textAlign='right';ctx.fillText(v.toPrecision(3),48*(k+1),y+3);});
  }
  if(!Number.isFinite(x0))return;const span=x1-x0||1;
  ctx.fillStyle=this.css('--dim');for(let j=0;j<=4;j++){ctx.textAlign=j===0?'left':j===4?'right':'center';ctx.fillText(((x1-x0)*j/4000).toFixed(1)+' s',left+pw*j/4,H-6);}
  for(const s of this.series){
   const [lo,hi]=ranges.get(s.unit);ctx.strokeStyle=s.color;ctx.lineWidth=2;ctx.beginPath();let pen=false;
   for(const p of s.points){if(!Number.isFinite(p.value)){pen=false;continue;}const x=left+pw*(p.t-x0)/span,y=top+ph*(1-(p.value-lo)/(hi-lo));pen?ctx.lineTo(x,y):ctx.moveTo(x,y);pen=true;}ctx.stroke();
  }
 }
}
