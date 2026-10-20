// SPDX-License-Identifier: GPL-3.0-or-later
// Derived from DL24 Bench, Copyright © 2026 project contributors.
// LabBridge: explicit sample sets and discontinuous traces.
export class CanvasChart{
  constructor(canvas,opts={}){
    this.c=canvas;this.maxpts=opts.maxpts||900;
    this.series=opts.series||[];          // [{key,cssVar}]
    this.css=opts.css||(v=>v);
    this.x=[];this.y={};this.vis={};
    for(const s of this.series){this.y[s.key]=[];this.vis[s.key]=true;}
  }
  setPoints(points){this.x=points.map((p,i)=>p.t??points[Math.max(0,i-1)]?.t??0);for(const s of this.series)this.y[s.key]=points.map(p=>p[s.key]);this.redraw();}
  fit(){const c=this.c,r=c.getBoundingClientRect(),dpr=devicePixelRatio||1;
    c.width=Math.max(320,r.width*dpr);c.height=Math.max(200,r.height*dpr);
    c.getContext('2d').setTransform(dpr,0,0,dpr,0,0);this.redraw();}
  pushSample(t,vals){this.x.push(t);
    for(const s of this.series)this.y[s.key].push(vals[s.key]);
    if(this.x.length>this.maxpts){this.x.shift();for(const s of this.series)this.y[s.key].shift();}
    this.redraw();}
  setVisible(key,on){this.vis[key]=on;this.redraw();}
  clear(){this.x.length=0;for(const s of this.series)this.y[s.key].length=0;this.redraw();}
  // Per-series y-range: each series scales to its own data min..max (6% margin,
  // clamped so the floor never dips below 0 when the data doesn't), so V/A/W
  // with very different magnitudes each use the full plot height (scope-style).
  _range(key){
    const arr=this.y[key];let lo=Infinity,hi=-Infinity;
    for(const v of arr){if(v===null||!Number.isFinite(v))continue;if(v<lo)lo=v;if(v>hi)hi=v;}
    if(!isFinite(lo)){lo=0;hi=1;}
    const dataLo=lo;
    if(hi-lo<1e-9){const p=Math.max(Math.abs(hi)*.05,.05);lo-=p;hi+=p;}
    else{const m=(hi-lo)*.06;lo-=m;hi+=m;}
    if(dataLo>=0&&lo<0)lo=0;
    return [lo,hi];
  }
  redraw(){
    const c=this.c;if(!c.getContext)return;const ctx=c.getContext('2d');
    const dpr=devicePixelRatio||1,W=c.width/dpr,H=c.height/dpr;
    ctx.clearRect(0,0,W,H);
    const active=this.series.filter(s=>this.vis[s.key]);
    // one axis-label column per visible series, in that series' colour, in
    // legend order — each column reads against its own scale
    const colW=38,padL=8+Math.max(1,active.length)*colW,padR=10,padT=36,padB=26;
    const pw=W-padL-padR,ph=H-padT-padB;
    const rng={};for(const s of active)rng[s.key]=this._range(s.key);
    const fmt=v=>{const a=Math.abs(v);return v.toFixed(a<10?2:a<100?1:0);};
    ctx.lineWidth=1;ctx.font='10px ui-monospace,monospace';
    ctx.textAlign='right';ctx.textBaseline='middle';
    for(let i=0;i<=4;i++){const y=padT+ph*i/4;
      ctx.strokeStyle=this.css('--grid');
      ctx.beginPath();ctx.moveTo(padL,y);ctx.lineTo(W-padR,y);ctx.stroke();
      active.forEach((s,j)=>{const [lo,hi]=rng[s.key];
        ctx.fillStyle=this.css(s.cssVar);
        ctx.fillText(fmt(hi-(hi-lo)*i/4),8+(j+1)*colW-6,y);});
    }
    const n=this.x.length;if(n<2)return;
    const x0=this.x[0],x1=this.x[n-1],xr=(x1-x0)||1;
    ctx.fillStyle=this.css('--dim');ctx.textBaseline='bottom';
    for(let j=0;j<=4;j++){ctx.textAlign=j===0?'left':j===4?'right':'center';ctx.fillText(((x1-x0)*j/4000).toFixed(1)+' s',padL+pw*j/4,H-2);}
    for(const s of active){const arr=this.y[s.key];const [lo,hi]=rng[s.key];const yr=(hi-lo)||1;
      ctx.strokeStyle=this.css(s.cssVar);ctx.lineWidth=2;ctx.beginPath();
      let pen=false;for(let i=0;i<n;i++){if(arr[i]===null||!Number.isFinite(arr[i])){pen=false;continue;}const px=padL+pw*(this.x[i]-x0)/xr,py=padT+ph*(1-(arr[i]-lo)/yr);
        pen?ctx.lineTo(px,py):ctx.moveTo(px,py);pen=true;}
      ctx.stroke();}
  }
}
