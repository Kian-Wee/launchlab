#include "../LaunchLabMini/motion_store.h"
#include <cassert>
#include <iostream>
static LaunchMotion::Trace trace(uint32_t number,float rpm) {
  LaunchMotion::Trace t;t.fused=true;t.quality=LaunchMotion::Quality::Valid;t.count=48;t.number=number;t.rpm=rpm;t.durationMs=400;t.gravity[2]=1;
  for(unsigned i=0;i<t.count;++i)t.points[i].ms=-500+int(i*1150/47);
  t.startLevel={StickS3Level::State::Valid,6,.2f,0};t.endLevel={StickS3Level::State::Valid,15,0,-.5f};
  return t;
}
int main() {
  LaunchMotion::Trace reference;MotionStore store;assert(store.begin(reference) && !reference.valid());
  auto first=trace(10,5000),second=trace(11,5500);assert(store.save(first) && store.save(second));
  const auto beforeRead=Preferences::data;
  LaunchMotion::Trace loaded;MotionStore reboot;assert(reboot.begin(loaded) && loaded.number==11 && loaded.rpm==5500);
  assert(Preferences::data==beforeRead); // loading retained captures must not rewrite/erase either slot
  assert(loaded.fused && loaded.startLevel.valid() && loaded.endLevel.valid() && loaded.startLevel.degrees==6 && loaded.endLevel.down==-.5f);
  Preferences::data["ll-motionref0"][100]^=1;
  LaunchMotion::Trace fallback;MotionStore recovery;assert(recovery.begin(fallback) && fallback.number==10);
  Preferences::failWrite=true;assert(!recovery.save(second));Preferences::failWrite=false;
  LaunchMotion::Trace retained;MotionStore reread;assert(reread.begin(retained) && retained.number==10);
  assert(recovery.save(second));MotionStore final;assert(final.begin(retained) && retained.number==11);
  MotionStore scratch;LaunchMotion::Trace ignored;assert(scratch.begin(ignored,"ll-mot-qa") && scratch.save(trace(90,9000)));
  MotionStore real;assert(real.begin(retained) && retained.number==11);
  Preferences::shortRead=true;assert(!real.save(first));Preferences::shortRead=false;
  // Failed verification does not mutate the selected in-memory reference.
  assert(retained.number==11);
  for(auto &entry:Preferences::data)if(entry.first.find("ll-motion")==0)entry.second[0]^=1;
  const auto damaged=Preferences::data;
  MotionStore broken;LaunchMotion::Trace empty;assert(!broken.begin(empty) && !broken.ready() && !empty.valid());
  assert(Preferences::data==damaged);
  std::cout<<"PASS: compact alternating reference storage, reboot, damaged newest fallback, failed write/readback, namespace isolation and non-destructive recovery\n";
}
