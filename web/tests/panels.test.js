import {test} from 'node:test';import assert from 'node:assert/strict';
import {panelIds, canEnable, registerPanel} from '../src/panels.js';
for(const [id,capability] of [['output','output'],['load','load.cc'],['source','source.cc_cv']])registerPanel({id,capability,render:()=>{}});
test('panels register by capability, independent of ID',()=>{
 assert.deepEqual(panelIds({capabilities:['load.cc','output']}),['output','load']);
 assert.deepEqual(panelIds({capabilities:['source.cc_cv','output']}),['output','source']);
});
test('unknown, stale and unleased states cannot enable output',()=>{
 assert.equal(canEnable({state:'ready',output:'unknown'},true),false);
 assert.equal(canEnable({state:'unavailable',output:'off'},true),false);
 assert.equal(canEnable({state:'ready',output:'off'},false),false);
 assert.equal(canEnable({state:'ready',output:'off'},true),true);
});

test('a new module registers without changing core rendering',()=>{registerPanel({id:'custom',capability:'custom.measure',render:()=>{}});assert.deepEqual(panelIds({capabilities:['custom.measure']}),['custom']);});
