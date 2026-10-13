import test from 'node:test';
import assert from 'node:assert/strict';
import {restartK150} from '../src/device-reset.js';
test('K150 restart asserts and releases reset with GPIO0 released and settling delays',async()=>{
  const calls=[];let reset=false;
  const transport={setDTR:async value=>calls.push(['DTR',value]),setRTS:async value=>{calls.push(['RTS',value]);reset=value;}};
  await restartK150(transport,async ms=>calls.push(['wait',ms,reset]));
  assert.deepEqual(calls,[['DTR',false],['RTS',true],['wait',200,true],['RTS',false],['wait',200,false]]);
});
test('a pulse-wait failure releases reset and propagates so manual restart is requested',async()=>{
  const calls=[],fault=new Error('wait cancelled');
  const transport={setDTR:async value=>calls.push(['DTR',value]),setRTS:async value=>calls.push(['RTS',value])};
  await assert.rejects(restartK150(transport,async()=>{throw fault;}),error=>error===fault);
  assert.deepEqual(calls,[['DTR',false],['RTS',true],['RTS',false]]);
});
test('signal failure propagates rather than claiming a successful restart',async()=>{
  for(const fail of ['DTR','RTS']){
    const transport={setDTR:async()=>{if(fail==='DTR')throw new Error('signal unavailable');},setRTS:async()=>{throw new Error('signal unavailable');}};
    await assert.rejects(restartK150(transport,async()=>{}),/signal unavailable/);
  }
});
