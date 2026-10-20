import {CoreApi} from '../generated/src/index';
import type {AuthStatus} from '../generated/src/index';

export class UiAuth {
  core!: CoreApi;
  state: AuthStatus = {passwordConfigured:false,setupAllowed:false,authenticated:false};
  loggingOut = false;
  onLost: () => void = () => {};
  beforeLogout: () => Promise<void> = async () => {};
  report: (error:unknown) => void = () => {};
  notify: (message:string) => void = () => {};

  fetch: typeof fetch = async (input, init) => {
    const authenticated = this.state.authenticated, csrfToken = this.state.csrfToken;
    const headers = new Headers(init?.headers);
    if (csrfToken && !['GET','HEAD'].includes(init?.method ?? 'GET'))
      headers.set('X-Lab-CSRF',csrfToken);
    const response = await fetch(input,{...init,headers,credentials:'same-origin'});
    // A public scan started before sign-in may finish after password autofill
    // or a successful login. Only the session used by this request can be lost.
    if (response.status === 401 && authenticated && this.state.authenticated &&
        csrfToken === this.state.csrfToken &&
        !new URL(String(input),location.origin).pathname.startsWith('/api/v1/auth/')) {
      this.state = {...this.state,authenticated:false,csrfToken:undefined};
      this.onLost(); // Display login; never replay the failed request.
    }
    return response;
  };

  async refresh() { this.state = await this.core.getAuthStatus(); }

  render(parent:HTMLElement, resume:() => Promise<void>) {
    const section=document.createElement('section'); parent.append(section);
    const text=(tag:string,value:string,target:HTMLElement=section) => {
      const node=document.createElement(tag);node.textContent=value;target.append(node);return node;
    };
    const password=(form:HTMLFormElement,label:string,name:string,autocomplete:HTMLInputElement['autocomplete']) => {
      const wrapper=text('label',label,form),field=document.createElement('input');
      field.type='password';field.name=name;field.autocomplete=autocomplete;field.required=true;
      wrapper.append(field);return field;
    };
    const submit=(form:HTMLFormElement,label:string,fn:() => Promise<void>) => {
      const button=document.createElement('button');button.type='submit';button.textContent=label;form.append(button);
      form.onsubmit=event => {
        event.preventDefault();button.disabled=true;
        void fn().catch(this.report).finally(()=>{button.disabled=false;});
      };
    };
    const confirm=(a:string,b:string) => {
      if(a!==b)throw Error('Password entries must match');
      if([...a].length<12 || [...a].length>128 || a.includes('\0'))
        throw Error('Password must contain 12..128 characters without NUL');
    };
    const identity=(form:HTMLFormElement) => {
      const field=document.createElement('input');field.type='text';field.name='username';
      field.autocomplete='username';field.value=location.hostname;field.hidden=true;form.append(field);
    };
    if(this.state.authenticated) {
      text('p','Signed in');
      const logout=document.createElement('button');logout.textContent='Sign out';section.append(logout);
      logout.onclick=()=>{
        logout.disabled=true;this.loggingOut=true;
        void (async()=>{
          let stopFailed=false;
          try { await this.beforeLogout(); } catch { stopFailed=true; }
          this.state=await this.core.logoutUi();
          this.onLost();
          this.notify(stopFailed?'Signed out. Shutdown could not be confirmed; lease expiry remains active. Verify outputs OFF.':'Signed out.');
        })().catch(this.report).finally(()=>{logout.disabled=false;this.loggingOut=false;});
      };
      const details=document.createElement('details');section.append(details);text('summary','Change password',details);
      const form=document.createElement('form');details.append(form);identity(form);
      const old=password(form,'Current device password','current-password','current-password');
      const next=password(form,'New device password','new-password','new-password');
      const again=password(form,'Confirm device password','confirm-password','new-password');
      submit(form,'Change password',async()=>{
        confirm(next.value,again.value);
        const request={currentPassword:old.value,password:next.value,confirmPassword:again.value};
        old.value='';next.value='';again.value='';
        this.notify('Saving password…');
        this.state=await this.core.changeUiPassword({uiPasswordChange:request});
        this.onLost();this.notify('Password changed. Sign in again; all browser sessions have ended.');
      });
      return;
    }
    if(!this.state.passwordConfigured) {
      text('h2','Set device password');
      if(!this.state.setupAllowed) {
        text('p','Choose the first device password at http://192.168.4.1 while connected to the LabBridge setup access point, or use trusted USB.');
        text('code','labbridge --endpoint serial:/dev/ttyACM0 ui-password');
        return;
      }
      const form=document.createElement('form');section.append(form);identity(form);
      const next=password(form,'New device password','new-password','new-password');
      const again=password(form,'Confirm device password','confirm-password','new-password');
      submit(form,'Set password',async()=>{
        confirm(next.value,again.value);
        const request={password:next.value,confirmPassword:again.value};next.value='';again.value='';
        this.notify('Saving password…');this.state=await this.core.setupUiPassword({uiPasswordSetup:request});
        await resume();this.notify('Password saved. Sign in with your device password.');
      });
      return;
    }
    text('h2','Sign in');
    const form=document.createElement('form');section.append(form);identity(form);
    const field=password(form,'Device password','password','current-password');
    submit(form,'Sign in',async()=>{
      const value=field.value;field.value='';this.notify('Signing in…');
      this.state=await this.core.loginUi({uiLogin:{password:value}});
      this.notify('Signed in.');await resume();
    });
    text('p','Forgot your password? Reset it over trusted USB with the ui-password command.');
  }
}
