#include "../LaunchLabMini/device_backup.h"
#include "../LaunchLabMini/appearance_store.h"
#include "../LaunchLabMini/motion_store.h"
#include "../LaunchLabMini/practice_store.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static LaunchMotion::Trace trace(uint32_t number,float rpm) {
  LaunchMotion::Trace t;t.fused=true;t.quality=LaunchMotion::Quality::Valid;t.count=48;t.number=number;t.rpm=rpm;t.durationMs=400;t.gravity[2]=1;
  for(unsigned i=0;i<t.count;++i){t.points[i].ms=-500+int(i*1150/47);t.points[i].q[0]=16384;}
  t.startLevel={StickS3Level::State::Valid,6,.2f,0};t.endLevel={StickS3Level::State::Valid,15,0,-.5f};
  return t;
}
static DeviceBackup::Contents sample() {
  DeviceBackup::Contents c;
  for(uint32_t i=1;i<=40;++i)c.practice.accept(i,true,5000+i*37,i*20000);
  c.practice.newSession();c.practice.accept(41,true,9100,900000);
  c.look.select(Appearance::Theme::Amber,Appearance::Effect::Lightning);
  c.bests.observe(0,0,true,8800);c.bests.observe(1,1,true,7700);
  c.hasMotion=true;c.motion=trace(41,9100);
  c.settings={7,60,RpmEstimator::Mode::SingleTurn,StickS3SensorProfile::Mode::Tcrt,true};
  return c;
}
static bool same(const DeviceBackup::Contents &a,const DeviceBackup::Contents &b) {
  PracticeHistory::Image x,y;a.practice.encode(x);b.practice.encode(y);
  if(x!=y || a.look.theme!=b.look.theme || a.look.effect!=b.look.effect || a.look.revision!=b.look.revision)return false;
  for(unsigned i=0;i<4;++i)if(a.bests.rpm[i]!=b.bests.rpm[i])return false;
  if(a.hasMotion!=b.hasMotion || (a.hasMotion && (a.motion.number!=b.motion.number || a.motion.rpm!=b.motion.rpm || a.motion.count!=b.motion.count)))return false;
  const auto &s=a.settings,&t=b.settings;
  return s.sleepMinutes==t.sleepMinutes && s.brightness==t.brightness && s.rpmMethod==t.rpmMethod && s.sensorProfile==t.sensorProfile && s.flipped==t.flipped;
}
// Sends text through the receiver the way the web updater does: lines, then ".".
static DeviceBackup::Receiver::Event transfer(DeviceBackup::Receiver &rx,const std::vector<std::string> &lines,uint32_t &now) {
  rx.start(now);auto last=DeviceBackup::Receiver::Event::None;
  for(const auto &line:lines)for(char c:line+"\r\n"){last=rx.feed(c,++now);if(last==DeviceBackup::Receiver::Event::Error)return last;}
  for(char c:std::string(".\n"))last=rx.feed(c,++now);
  return last;
}

int main() {
  using namespace DeviceBackup;
  const auto original=sample();
  Buffer buffer;const size_t n=encode(original,"0.11.0-sticks3-test",buffer);
  assert(n<=MAX_BYTES && get32(buffer.data()+8)==n);
  Contents decoded;assert(decode(buffer.data(),n,decoded) && same(original,decoded));
  assert(std::string(decoded.runtime)=="0.11.0-sticks3-test");

  // Expanded accents travel through the complete USB backup, including the
  // highest new ID and either saved Flip switch value.
  for(unsigned theme=0;theme<Appearance::THEMES;++theme)for(bool flipped:{false,true}) {
    auto trial=original;trial.settings.flipped=flipped;
    assert(trial.look.select(Appearance::Theme(theme),Appearance::Effect::Lightning));
    Buffer encoded;const auto length=encode(trial,"0.12.1-sticks3-test",encoded);
    Contents restored;assert(decode(encoded.data(),length,restored) && same(trial,restored));
  }

  // Base64 lines through the receiver reproduce the exact bytes.
  std::vector<std::string> lines;base64Lines(buffer.data(),n,96,[&](const char *line){lines.push_back(line);});
  for(const auto &line:lines)assert(line.size()<=96);
  uint32_t now=0;Receiver rx;
  assert(transfer(rx,lines,now)==Receiver::Event::Complete && !rx.active());
  std::array<uint8_t,MAX_BYTES> back{};size_t written=0;
  assert(rx.take(back.data(),back.size(),written) && written==n && std::equal(back.begin(),back.begin()+n,buffer.begin()));
  // Any line split works: the device concatenates before decoding.
  std::string all;for(const auto &line:lines)all+=line;
  std::vector<std::string> odd;for(size_t i=0;i<all.size();i+=37)odd.push_back(all.substr(i,37));
  assert(transfer(rx,odd,now)==Receiver::Event::Complete && rx.take(back.data(),back.size(),written) && written==n);

  // No motion yet (a fresh device) is still a complete backup.
  auto fresh=original;fresh.hasMotion=false;
  Buffer small;const size_t m=encode(fresh,"x",small);assert(m==n-8-LaunchMotion::IMAGE_BYTES);
  Contents noMotion;assert(decode(small.data(),m,noMotion) && !noMotion.hasMotion && same(fresh,noMotion));

  // Every corruption is rejected before anything could be written.
  Contents rejected=sample();rejected.look.select(Appearance::Theme::Mint,Appearance::Effect::Off);
  const auto untouched=rejected;
  for(size_t i=0;i<n;i+=7){auto bad=buffer;bad[i]^=0x40;assert(!decode(bad.data(),n,rejected));}
  assert(same(rejected,untouched));
  assert(!decode(buffer.data(),n-1,rejected) && !decode(buffer.data(),HEADER_BYTES,rejected));
  {
    auto bad=buffer;put32(bad.data()+4,2);put32(bad.data()+n-4,checksum(bad.data(),n-4));assert(!decode(bad.data(),n,rejected));
  }
  {
    // A valid checksum cannot rescue an invalid setting.
    auto bad=sample();bad.settings.brightness=45;Buffer out;const size_t k=encode(bad,"x",out);assert(!decode(out.data(),k,rejected));
    bad=sample();bad.settings.sleepMinutes=0;encode(bad,"x",out);assert(!decode(out.data(),k,rejected));
  }
  {
    // Missing required section: drop the settings section and re-seal.
    Buffer out=buffer;const size_t cut=n-4-8-SETTINGS_BYTES;put32(out.data()+8,uint32_t(cut+4));
    put32(out.data()+cut,checksum(out.data(),cut));assert(!decode(out.data(),cut+4,rejected));
  }
  {
    // Duplicate section: repeat the settings section.
    Buffer out{};std::copy(buffer.begin(),buffer.begin()+n-4,out.begin());
    const size_t settings=n-4-8-SETTINGS_BYTES;
    if(n+8+SETTINGS_BYTES<=MAX_BYTES) {
      std::copy(buffer.begin()+settings,buffer.begin()+n-4,out.begin()+n-4);
      const size_t k=n+8+SETTINGS_BYTES;put32(out.data()+8,uint32_t(k));put32(out.data()+k-4,checksum(out.data(),k-4));
      assert(!decode(out.data(),k,rejected));
    }
  }

  // Receiver bounds: stray characters, oversize, a bad terminator and silence.
  rx.start(0);for(char c:std::string("QUJD\n"))rx.feed(c,1);
  assert(rx.feed('#',2)==Receiver::Event::Error && std::string(rx.error())=="bad_character" && !rx.active());
  rx.start(0);assert(rx.feed('.',1)==Receiver::Event::None && rx.feed('x',2)==Receiver::Event::Error);
  rx.start(0);auto event=Receiver::Event::None;
  for(unsigned i=0;i<=TEXT_BYTES && event!=Receiver::Event::Error;++i)event=rx.feed('A',1);
  assert(event==Receiver::Event::Error && std::string(rx.error())=="too_large");
  rx.start(100);assert(!rx.timedOut(100+Receiver::IDLE_MS-1) && rx.timedOut(100+Receiver::IDLE_MS));
  rx.start(0);assert(rx.feed('Q',1)==Receiver::Event::None && rx.feed('\n',2)==Receiver::Event::Chunk && rx.received()==1);
  assert(rx.feed('.',3)==Receiver::Event::None && rx.feed('\n',4)==Receiver::Event::Complete && !rx.take(back.data(),back.size(),written));

  // Restoring an older backup onto a device whose stores hold newer generations:
  // the restored records must be the ones loaded after the restart.
  Preferences::data.clear();
  PracticeHistory live;PracticeStore practiceStore;assert(practiceStore.begin(live));
  for(uint32_t i=1;i<=60;++i){live.accept(i,true,6000,i*1000);assert(practiceStore.save(live));}
  Appearance::Config look;AppearanceStore<Appearance::Config> lookStore;assert(lookStore.begin(look,"ll-appearance","cfg0","cfg1"));
  for(unsigned i=0;i<9;++i){look.select(Appearance::Theme(i%4),Appearance::Effect(i%6));assert(lookStore.save(look));}
  Appearance::Bests bests;AppearanceStore<Appearance::Bests> bestStore;assert(bestStore.begin(bests,"ll-bests","best0","best1"));
  for(unsigned i=0;i<12;++i){bests.observe(0,0,true,9000+i);assert(bestStore.save(bests));}
  LaunchMotion::Trace reference;MotionStore motionStore;assert(motionStore.begin(reference));
  assert(motionStore.save(trace(55,6000)) && motionStore.save(trace(60,6000)));
  auto restore=decoded;
  assert(restore.practice.generation()<live.generation() && restore.look.revision<look.revision && restore.bests.revision<bests.revision);
  restore.practice.supersede(live.generation());restore.look.revision=look.revision+1;restore.bests.revision=bests.revision+1;
  assert(practiceStore.save(restore.practice) && lookStore.save(restore.look) && bestStore.save(restore.bests) && motionStore.save(restore.motion));
  PracticeHistory afterHistory;PracticeStore p2;assert(p2.begin(afterHistory) && afterHistory.size()==41 && afterHistory.recent()->rpm==9100);
  Appearance::Config afterLook;AppearanceStore<Appearance::Config> l2;assert(l2.begin(afterLook,"ll-appearance","cfg0","cfg1"));
  assert(afterLook.theme==Appearance::Theme::Amber && afterLook.effect==Appearance::Effect::Lightning);
  Appearance::Bests afterBests;AppearanceStore<Appearance::Bests> b2;assert(b2.begin(afterBests,"ll-bests","best0","best1"));
  assert(afterBests.rpm[0]==8800 && afterBests.rpm[3]==7700);
  LaunchMotion::Trace afterMotion;MotionStore m2;assert(m2.begin(afterMotion) && afterMotion.number==41);
  // Restoring the same backup twice is harmless: it supersedes again.
  restore.practice.supersede(afterHistory.generation());assert(p2.save(restore.practice));
  PracticeHistory again;PracticeStore p3;assert(p3.begin(again) && again.size()==41);

  std::cout<<"PASS: device backup round trip, base64 line transfer at any split, fresh device without motion, all-or-nothing validation, receiver bounds, and restored records outrank newer saved generations\n";
}
