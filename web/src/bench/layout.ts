// SPDX-License-Identifier: GPL-3.0-or-later
/** Presentation only: dialogs and the mobile drawer never change ownership. */
export class BenchLayout {
 private returnFocus=new Map<string,HTMLElement>();
 private mobile=matchMedia('(max-width: 820px)');
 constructor(){
  this.resizeMeasurements();
  document.querySelectorAll<HTMLElement>('[data-open-dialog]').forEach(button=>button.onclick=()=>this.toggle(button.dataset.openDialog!,button));
  document.querySelectorAll<HTMLElement>('[data-close-dialog]').forEach(button=>button.onclick=()=>this.close(button.dataset.closeDialog!));
  document.getElementById('moreMenu')!.onclick=()=>this.more(document.getElementById('globalActions')!.dataset.open!=='true');
  document.getElementById('globalActions')!.addEventListener('click',event=>{if((event.target as HTMLElement).closest('button'))this.more(false);});
  document.getElementById('devicePanelToggle')!.onclick=()=>this.drawer(document.body.dataset.devicePanel!=='open');
  document.getElementById('devicePanelClose')!.onclick=()=>this.drawer(false);
  document.getElementById('devicePanelBackdrop')!.onclick=()=>this.drawer(false);
  this.mobile.addEventListener('change',()=>{this.drawer(false);this.more(false);});
  document.addEventListener('keydown',event=>{
   if(event.key!=='Escape'||!document.getElementById('settingsModal')!.hidden)return;
   const open=Array.from(document.querySelectorAll<HTMLDialogElement>('dialog[open]')).slice(-1)[0];
   if(open){event.preventDefault();this.close(open.id);}
   else if(document.getElementById('globalActions')!.dataset.open==='true'){event.preventDefault();this.more(false);document.getElementById('moreMenu')!.focus();}
   else if(document.body.dataset.devicePanel==='open'){event.preventDefault();this.drawer(false);}
  });
  document.addEventListener('pointerdown',event=>{
   const target=event.target as HTMLElement;
   if(target.closest('[data-open-dialog]'))return;
   for(const dialog of Array.from(document.querySelectorAll<HTMLDialogElement>('dialog[open]')))if(!dialog.contains(target))this.close(dialog.id,false);
   if(!target.closest('#globalActions,#moreMenu'))this.more(false);
  });
  // A modeless panel keeps priority STOP reachable. Tab remains contained while
  // using a panel; closing it restores its header trigger.
  document.addEventListener('keydown',event=>{
   if(event.key!=='Tab')return;
   const active=document.activeElement as HTMLElement|null,dialog=active?.closest<HTMLDialogElement>('dialog[open]');
   if(!dialog)return;
   const controls=Array.from(dialog.querySelectorAll<HTMLElement>('button:not(:disabled),input:not(:disabled),select:not(:disabled),a[href],summary')).filter(el=>el.getClientRects().length>0);
   const first=controls[0],last=controls.slice(-1)[0];
   if(event.shiftKey&&active===first){event.preventDefault();last?.focus();}else if(!event.shiftKey&&active===last){event.preventDefault();first?.focus();}
  });
 }
 private resizeMeasurements(){
  const main=document.querySelector<HTMLElement>('.main')!,dock=document.getElementById('measurementDock')!,handle=document.getElementById('measurementResize')!;
  const storageKey='bench:measurement-height',minimum=64;
  let preferred:number|undefined,maximum=144,drag:{id:number;y:number;height:number}|undefined;
  try{const saved=Number(localStorage.getItem(storageKey));if(Number.isFinite(saved)&&saved>=minimum)preferred=saved;}catch{}
  const update=()=>{
   const style=getComputedStyle(main);
   // Retain 160 px of plot plus its border. A smaller viewport clamps
   // the displayed height without discarding the user's preferred height.
   maximum=Math.max(minimum,Math.floor(main.clientHeight-parseFloat(style.paddingTop)-parseFloat(style.paddingBottom)-parseFloat(style.rowGap)-162));
   dock.style.setProperty('--measurement-max',maximum+'px');
   if(preferred!==undefined)dock.style.setProperty('--measurement-height',Math.min(maximum,Math.max(minimum,preferred))+'px');
   handle.setAttribute('aria-valuemin',String(minimum));handle.setAttribute('aria-valuemax',String(maximum));
   const current=Math.round(dock.getBoundingClientRect().height);handle.setAttribute('aria-valuenow',String(current));handle.setAttribute('aria-valuetext',`${current} pixels`);
  };
  const set=(height:number)=>{preferred=Math.max(minimum,Math.min(maximum,height));update();try{localStorage.setItem(storageKey,String(preferred));}catch{}};
  handle.addEventListener('pointerdown',event=>{
   if(event.button!==0)return;event.preventDefault();handle.focus();
   drag={id:event.pointerId,y:event.clientY,height:dock.getBoundingClientRect().height};handle.setPointerCapture(event.pointerId);document.body.classList.add('measurement-resizing');
  });
  handle.addEventListener('pointermove',event=>{if(drag?.id===event.pointerId)set(drag.height+drag.y-event.clientY);});
  const finish=(event:PointerEvent)=>{if(drag?.id!==event.pointerId)return;drag=undefined;document.body.classList.remove('measurement-resizing');};
  handle.addEventListener('pointerup',finish);handle.addEventListener('pointercancel',finish);handle.addEventListener('lostpointercapture',finish);
  handle.addEventListener('keydown',event=>{
   const current=dock.getBoundingClientRect().height;
   const height=event.key==='ArrowUp'?current+20:event.key==='ArrowDown'?current-20:event.key==='Home'?minimum:event.key==='End'?maximum:undefined;
   if(height===undefined)return;event.preventDefault();set(height);
  });
  new ResizeObserver(update).observe(main);new ResizeObserver(update).observe(dock);update();
 }
 open(id:string,trigger=document.getElementById('accountMenu')!){
  document.getElementById('settingsModal')!.hidden=true;
  this.closeMenus();this.returnFocus.set(id,trigger);(document.getElementById(id) as HTMLDialogElement).show();this.more(false);
 }
 toggle(id:string,trigger:HTMLElement){const dialog=document.getElementById(id) as HTMLDialogElement;if(dialog.open)this.close(id);else this.open(id,trigger);}
 close(id:string,restore=true){
  const dialog=document.getElementById(id) as HTMLDialogElement;if(!dialog.open)return;dialog.close();
  if(restore){const trigger=this.returnFocus.get(id);if(trigger?.getClientRects().length)trigger.focus();else document.getElementById('moreMenu')?.focus();}
 }
 closeMenus(){for(const dialog of Array.from(document.querySelectorAll<HTMLDialogElement>('dialog[open]')))this.close(dialog.id,false);}
 account(authenticated:boolean){
  if(!authenticated){document.getElementById('settingsModal')!.hidden=true;this.drawer(false);}
  if(authenticated)this.close('accountDialog',false);
  else if(!(document.getElementById('accountDialog') as HTMLDialogElement).open){this.open('accountDialog');document.querySelector<HTMLInputElement>('#benchAuth input:not([hidden])')?.focus();}
 }
 private more(open:boolean){document.getElementById('globalActions')!.dataset.open=String(open);document.getElementById('moreMenu')!.setAttribute('aria-expanded',String(open));}
 private drawer(open:boolean){
  const main=document.querySelector('.main')!;if(open)main.setAttribute('inert','');else main.removeAttribute('inert');
  document.body.dataset.devicePanel=open?'open':'closed';document.getElementById('devicePanelBackdrop')!.hidden=!open;
  document.getElementById('devicePanelToggle')!.setAttribute('aria-expanded',String(open));
  if(open)document.getElementById('deviceSelect')!.focus();else if(this.mobile.matches)document.getElementById('devicePanelToggle')!.focus();
 }
}
