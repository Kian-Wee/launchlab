import test from 'node:test';
import assert from 'node:assert/strict';
import {createInstaller} from '../src/installer.js';
import {catalog,oldCatalog,base,localFetch,fake,md5,images} from './helpers.js';
function setup({loader=fake(),requestPort=async()=>({}),fetchFn=localFetch,disconnect=async()=>{},resetError=null}={}) {
  const state={choosers:0,opens:0,closes:0,busy:[],statuses:[],recovery:[],devices:[],signals:[]};
  const installer=createInstaller({baseUrl:base,md5,fetchFn,
    requestPort:async()=>{state.choosers++;return requestPort();},
    openConnection:async()=>{state.opens++;return {loader,transport:{setDTR:async value=>{state.signals.push(['DTR',value]);if(resetError)throw resetError;},setRTS:async value=>{state.signals.push(['RTS',value]);},disconnect:async()=>{state.closes++;await disconnect();}}};},
    onBusy:b=>state.busy.push(b),onStatus:(message,result)=>state.statuses.push({message,result}),onRecovery:r=>state.recovery.push(r),onDevice:d=>state.devices.push(d)});
  return {installer,state,loader};
}
test('cancelled chooser resets busy and does not open a device',async()=>{
  const s=setup({requestPort:async()=>{throw Object.assign(new Error('cancelled'),{name:'NotFoundError'});}});
  assert.equal((await s.installer.run({catalog,mode:'update'})).status,'cancelled');
  assert.equal(s.state.opens,0);assert.equal(s.installer.busy,false);assert.deepEqual(s.state.busy,[true,false]);
});
test('download failure does not open USB and a subsequent attempt starts cleanly',async()=>{
  let fail=true;const s=setup({fetchFn:async url=>fail?{ok:false,status:503}:localFetch(url)});
  assert.equal((await s.installer.run({catalog,mode:'update'})).status,'error');assert.equal(s.state.opens,0);
  fail=false;assert.equal((await s.installer.run({catalog,mode:'update'})).status,'verified');assert.equal(s.state.closes,1);
});
test('double clicks cannot start another chooser or overlapping write',async()=>{
  let choose;const s=setup({requestPort:()=>new Promise(resolve=>{choose=resolve;})});
  const first=s.installer.run({catalog,mode:'update'});assert.equal(s.installer.busy,true);
  assert.equal((await s.installer.run({catalog,mode:'update'})).status,'busy');assert.equal(s.state.choosers,1);
  choose({});assert.equal((await first).status,'verified');assert.equal(s.loader.writes.length,1);assert.equal(s.installer.busy,false);
});
test('connection failure closes transport and allows a fresh retry',async()=>{
  const l=fake();let fail=true;const resets=[];l.main=async mode=>{resets.push(mode);if(fail)throw new Error('connection lost');};const s=setup({loader:l});
  const failed=await s.installer.run({catalog,mode:'update'});
  assert.equal(failed.status,'error');assert.equal(failed.phase,'connect');assert.equal(failed.writeStarted,false);
  assert.match(failed.advice,/green LED flashes/);assert.equal(s.state.closes,1);assert.equal(l.writes.length,0);
  fail=false;assert.equal((await s.installer.run({catalog,mode:'update'})).status,'verified');assert.equal(s.state.closes,2);
  assert.deepEqual(resets,['default_reset','default_reset']);
});
test('all install modes reset into the bootloader before inspecting or writing flash',async()=>{
  for(const mode of ['update','install','recovery']){
    const l=fake();let connected=false;
    l.main=async reset=>{assert.equal(reset,'default_reset');assert.equal(l.writes.length,0);connected=true;};
    const read=l.readFlash;l.readFlash=async(...args)=>{assert.equal(connected,true);return read.apply(l,args);};
    const s=setup({loader:l});
    assert.equal((await s.installer.run({catalog,mode,installConfirmed:true,recoveryConfirmed:true})).status,'verified');
    assert.equal(s.state.closes,1);
  }
});
test('write interruption is never reported as success; repair retry closes each connection',async()=>{
  const l=fake();const write=l.writeFlash;let fail=true;
  l.writeFlash=async function(o){await write.call(this,o);if(fail){this.app[10000]^=1;throw new Error('USB disconnected mid-write');}};
  const s=setup({loader:l});const interrupted=await s.installer.run({catalog,mode:'update'});
  assert.equal(interrupted.status,'error');assert.equal(s.state.closes,1);assert.equal(s.installer.busy,false);
  fail=false;assert.equal((await s.installer.run({catalog,mode:'update'})).status,'error');
  assert.equal((await s.installer.run({catalog,mode:'recovery',recoveryConfirmed:true})).status,'verified');
  assert.equal((await s.installer.run({catalog,mode:'update'})).status,'up-to-date');assert.equal(l.writes.length,2);assert.equal(s.state.closes,4);
});
test('reset failure after verified write requests manual restart and keeps success evidence',async()=>{
  const l=fake();const s=setup({loader:l,resetError:new Error('reset unavailable')});
  const result=await s.installer.run({catalog,mode:'update'});assert.equal(result.status,'verified');assert.equal(result.restarted,false);
  assert.match(s.state.statuses.at(-1).message,/Power\/Reset/);assert.equal(s.state.statuses.at(-1).result,'success');assert.equal(s.state.closes,1);
});
test('older selected release is blocked end-to-end before write',async()=>{
  const s=setup({loader:fake({app:images[3].data})});const result=await s.installer.run({catalog:oldCatalog,mode:'update'});
  assert.equal(result.status,'error');assert.match(result.error.message,/Downgrade blocked/);assert.equal(s.loader.writes.length,0);assert.equal(s.state.closes,1);
});
test('missing explicit repair or first-install consent stops before chooser',async()=>{
  for(const mode of ['install','recovery']){const s=setup();assert.equal((await s.installer.run({catalog,mode})).status,'error');assert.equal(s.state.choosers,0);}
});
test('disconnect failure cannot retain busy state or trigger another write',async()=>{
  const s=setup({disconnect:async()=>{throw new Error('already unplugged');}});
  assert.equal((await s.installer.run({catalog,mode:'update'})).status,'verified');assert.equal(s.installer.busy,false);
  assert.equal((await s.installer.run({catalog,mode:'update'})).status,'up-to-date');assert.equal(s.loader.writes.length,1);
});
test('write interruption reports possible incomplete application and deliberate repair; never claims untouched flash',async()=>{
  const l=fake();l.writeFlash=async()=>{throw new Error('USB disconnected');};const s=setup({loader:l});
  const r=await s.installer.run({catalog,mode:'update'});
  assert.equal(r.phase,'write');assert.equal(r.writeStarted,true);assert.match(r.advice,/did not finish/);assert.match(r.advice,/Repair \/ rollback/);
  assert.doesNotMatch(r.advice,/No flash was written/);assert.equal(s.state.recovery.at(-1).writeStarted,true);assert.equal(s.state.closes,1);
});
test('interrupted First install gives four-image retry guidance and still requires new consent',async()=>{
  const l=fake();l.writeFlash=async()=>{throw new Error('MD5 mismatch');};const s=setup({loader:l});
  const r=await s.installer.run({catalog,mode:'install',installConfirmed:true});
  assert.equal(r.writeStarted,true);assert.match(r.advice,/bootloader, layout or application/);assert.match(r.advice,/Retry First install only/);
  const choosers=s.state.choosers;await s.installer.run({catalog,mode:'install'});assert.equal(s.state.choosers,choosers);assert.equal(s.state.closes,1);
});
test('layout refusal gives stop guidance before writing with no implicit First install',async()=>{
  const p=images[1].data.slice();p[0]^=1;const s=setup({loader:fake({partitions:p})});
  const r=await s.installer.run({catalog,mode:'update'});
  assert.equal(r.writeStarted,false);assert.equal(r.phase,'preflight');assert.match(r.advice,/No flash was written/);assert.match(r.advice,/not an automatic fix/);
  assert.equal(s.loader.writes.length,0);assert.equal(s.state.closes,1);
});
test('unreadable capacity is a preflight refusal and each retry reopens and rechecks',async()=>{
  const l=fake();let fail=true,checks=0;l.detectFlashSize=async()=>{checks++;if(fail)throw new Error('flash ID unavailable');return '8MB';};const s=setup({loader:l});
  const r=await s.installer.run({catalog,mode:'update'});assert.equal(r.writeStarted,false);assert.match(r.advice,/No flash was written/);assert.equal(l.writes.length,0);
  fail=false;assert.equal((await s.installer.run({catalog,mode:'update'})).status,'verified');assert.equal(checks,2);assert.equal(s.state.closes,2);
  assert.deepEqual(s.state.devices,[{chip:'ESP32-S3',flashSize:'8MB'}]);assert.equal(s.state.recovery.at(-1),null);
});
test('NotFoundError after chooser is a failure, not a harmless cancelled selection',async()=>{
  const l=fake();l.writeFlash=async()=>{throw Object.assign(new Error('device disappeared'),{name:'NotFoundError'});};const s=setup({loader:l});
  const r=await s.installer.run({catalog,mode:'update'});assert.equal(r.status,'error');assert.equal(r.writeStarted,true);assert.match(r.advice,/did not finish/);assert.equal(s.state.closes,1);
});

 test('successful writes and same-image no-ops pulse reset before closing USB',async()=>{
  for(const app of [undefined,images[3].data]){
    const s=setup({loader:fake({app})});
    const result=await s.installer.run({catalog,mode:'update'});
    assert.equal(result.restarted,true);
    assert.deepEqual(s.state.signals,[['DTR',false],['RTS',true],['RTS',false]]);
    assert.equal(s.state.closes,1);
  }
});
