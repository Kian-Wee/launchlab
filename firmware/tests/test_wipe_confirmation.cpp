#include "../LaunchLabMini/wipe_confirmation.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

using Stage=WipeConfirmation::Stage;
static uint32_t after(uint32_t start,uint32_t delta){return uint32_t(start+delta);}
static bool holdFor(WipeConfirmation &wipe,uint32_t start,uint32_t duration) {
  for(uint32_t held=100;held<duration;held+=100)assert(!wipe.tick(after(start,held),true,false));
  return wipe.tick(after(start,duration),true,false);
}
static void release(WipeConfirmation &wipe,uint32_t now) {
  assert(!wipe.tick(now,false,false));assert(!wipe.awaitingRelease());
}
static void arm(WipeConfirmation &wipe,uint32_t now) {
  wipe.begin(now);assert(wipe.active() && wipe.stage()==Stage::Warning && wipe.awaitingRelease());
  assert(!wipe.confirm(now));release(wipe,now);
  assert(wipe.confirm(now) && wipe.stage()==Stage::Confirm && wipe.awaitingRelease());
  assert(!wipe.confirm(now));release(wipe,now);
  assert(wipe.confirm(now) && wipe.stage()==Stage::Armed && wipe.awaitingRelease());
  assert(!wipe.confirm(now));
}
static void testCompletion(uint32_t start) {
  WipeConfirmation wipe;assert(!wipe.active() && !wipe.tick(start,true,false) && !wipe.confirm(start));
  arm(wipe,start);
  // A held while entering Armed cannot become the final hold, even after 3s.
  assert(!wipe.tick(start,true,false));
  assert(!wipe.tick(after(start,3000),true,false));assert(wipe.elapsed(after(start,3000))==0);
  release(wipe,after(start,3001));
  const uint32_t pressed=after(start,3002);
  assert(!wipe.tick(pressed,true,false) && wipe.elapsed(pressed)==0);
  assert(!holdFor(wipe,pressed,2999));assert(wipe.elapsed(after(pressed,2999))==2999);
  assert(wipe.tick(after(pressed,3000),true,false));assert(!wipe.active());
  assert(wipe.elapsed(after(pressed,3000))==0 && !wipe.awaitingRelease());
  for(unsigned i=0;i<10;++i)assert(!wipe.tick(after(pressed,3001+i),true,false));
  assert(!wipe.confirm(after(pressed,3010)));
}
static void testReleaseAndReentry() {
  WipeConfirmation wipe;arm(wipe,0);release(wipe,1);
  assert(!wipe.tick(2,true,false));assert(!holdFor(wipe,2,2500) && wipe.elapsed(2502)==2500);
  assert(!wipe.tick(2503,false,false) && wipe.elapsed(2503)==0);
  assert(!wipe.tick(2504,true,false));assert(!holdFor(wipe,2504,2999));
  assert(wipe.tick(5504,true,false));
  // Re-entry does not reuse progress or the still-down physical A button.
  arm(wipe,6000);assert(!wipe.tick(10000,true,false) && wipe.elapsed(10000)==0);
  release(wipe,10001);assert(!wipe.tick(10002,true,false));
  wipe.begin(11000);assert(wipe.stage()==Stage::Warning && wipe.awaitingRelease());
  assert(!wipe.tick(15000,true,false) && !wipe.confirm(15000));
  release(wipe,15001);assert(wipe.confirm(15002) && wipe.stage()==Stage::Confirm);
  wipe.cancel();assert(!wipe.active() && !wipe.tick(20000,true,false));
}
static void testCancellation() {
  for(unsigned stage=0;stage<3;++stage)for(bool aDown:{false,true}) {
    WipeConfirmation wipe;wipe.begin(0);release(wipe,0);
    for(unsigned i=0;i<stage;++i){assert(wipe.confirm(0));release(wipe,0);}
    assert(!wipe.tick(1,aDown,true) && !wipe.active());
    assert(!wipe.tick(4000,true,false));
  }
  WipeConfirmation wipe;arm(wipe,0);release(wipe,1);assert(!wipe.tick(2,true,false));
  assert(!holdFor(wipe,2,2999));
  assert(!wipe.tick(3002,true,true) && !wipe.active()); // Both buttons at completion cancel.
  arm(wipe,4000);assert(!wipe.tick(4001,false,true) && !wipe.active()); // B while awaiting release cancels.
}
static void testTimeout(uint32_t start) {
  WipeConfirmation wipe;arm(wipe,start);release(wipe,start);
  assert(!wipe.tick(after(start,29999),false,false) && wipe.active());
  assert(!wipe.tick(after(start,30000),false,false) && !wipe.active());
  wipe.begin(start);release(wipe,start);
  assert(!wipe.confirm(after(start,30000)) && !wipe.active());
  arm(wipe,start);release(wipe,start);
  assert(!wipe.tick(after(start,27000),true,false));
  assert(!holdFor(wipe,after(start,27000),2999));
  assert(!wipe.tick(after(start,30000),true,false) && !wipe.active()); // Timeout wins over simultaneous 3s completion.
  arm(wipe,start);release(wipe,start);
  assert(!wipe.tick(after(start,26000),true,false));assert(holdFor(wipe,after(start,26000),3000));
}
static void testFreshnessAndPollGaps(uint32_t start) {
  WipeConfirmation wipe;arm(wipe,start);release(wipe,start);
  assert(!wipe.tick(after(start,1),true,false));assert(!holdFor(wipe,after(start,1),2999));
  assert(!wipe.tick(after(start,3001),true,false,false));
  assert(wipe.active() && wipe.awaitingRelease() && wipe.elapsed(after(start,3001))==0);
  assert(!wipe.tick(after(start,3002),false,false,false) && wipe.awaitingRelease()); // Cached up is not release.
  assert(!wipe.tick(after(start,6002),true,false) && wipe.awaitingRelease());
  release(wipe,after(start,6003));assert(!wipe.tick(after(start,6004),true,false));
  assert(holdFor(wipe,after(start,6004),3000));

  arm(wipe,start);release(wipe,start);assert(!wipe.tick(after(start,1),true,false));
  assert(!wipe.tick(after(start,251),true,false) && wipe.elapsed(after(start,251))==250); // Exact gap limit allowed.
  assert(wipe.elapsed(after(start,502))==0); // Progress is not credited past an unseen interval.
  assert(!wipe.tick(after(start,502),true,false) && wipe.awaitingRelease());
  assert(!wipe.tick(after(start,3502),true,false) && wipe.elapsed(after(start,3502))==0);
  release(wipe,after(start,3503));assert(!wipe.tick(after(start,3504),true,false));
  assert(!wipe.tick(after(start,7004),true,false) && wipe.awaitingRelease()); // A 3.5s stall is not a hold.
  release(wipe,after(start,7005));assert(!wipe.tick(after(start,7006),true,false));
  assert(holdFor(wipe,after(start,7006),3000));

  arm(wipe,start);assert(!wipe.tick(after(start,30000),false,false,false) && !wipe.active());
  arm(wipe,start);assert(!wipe.tick(after(start,1),true,true,false) && !wipe.active());
  wipe.begin(start);assert(!wipe.tick(start,false,false,false) && !wipe.confirm(start));
  release(wipe,start);assert(wipe.confirm(start));
}
static void testHeldEntryAndCommandIsolation() {
  WipeConfirmation wipe;wipe.begin(0);
  for(uint32_t now=0;now<=4000;now+=100) {
    assert(!wipe.tick(now,true,false));assert(!wipe.confirm(now));
    assert(wipe.stage()==Stage::Warning && wipe.elapsed(now)==0);
  }
  release(wipe,4001);assert(wipe.confirm(4002));
  assert(!wipe.tick(4003,true,false) && !wipe.confirm(4003));
  release(wipe,4004);assert(wipe.confirm(4005));
  assert(!wipe.tick(8005,true,false) && wipe.elapsed(8005)==0);release(wipe,8006);
  // Repeated command-only confirmations and synthetic hold callbacks have no
  // erase API. Without a physical down state, tick can never commit.
  for(uint32_t now=8007;now<30000;now+=137) {
    assert(!wipe.confirm(now));assert(!wipe.tick(now,false,false));assert(wipe.elapsed(now)==0);
  }
  assert(!wipe.tick(30000,false,false) && !wipe.active());
}
int main() {
  testCompletion(0);testCompletion(std::numeric_limits<uint32_t>::max()-3500);
  testReleaseAndReentry();testCancellation();
  testTimeout(0);testTimeout(std::numeric_limits<uint32_t>::max()-15000);
  testFreshnessAndPollGaps(0);testFreshnessAndPollGaps(std::numeric_limits<uint32_t>::max()-3500);
  testHeldEntryAndCommandIsolation();
  std::cout<<"PASS: two released confirmations, fresh continuous 3s physical hold, one-shot completion, cancellation, interruption, absolute timeout, re-entry, command isolation, stale updates/poll gaps and uint32 wrap\n";
}
