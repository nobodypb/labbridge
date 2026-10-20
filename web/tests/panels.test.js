import {test} from 'node:test';import assert from 'node:assert/strict';
import {panelIds, canEnable} from '../src/panels.js';
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
