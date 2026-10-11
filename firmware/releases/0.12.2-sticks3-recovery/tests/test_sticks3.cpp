#include "../LaunchLabMini/sensor_profile.h"
#include "../LaunchLabMini/sample_ring.h"
#include <cassert>
#include <cmath>
#include <iostream>
using D=AnalogTachometer;
static void level(D &d,int value,uint32_t us) {
  for(uint32_t i=0;i<us/20;++i)d.feed(value);
}
static D ready() {
  D d;StickS3SensorProfile::configure(d);d.singleTurnPeak=true;level(d,140,1200000);
  assert(d.phase==D::Phase::Ready);return d;
}
static void square(D &d,uint32_t period,unsigned count,int amplitude=100) {
  for(unsigned i=0;i<count;++i){level(d,140+amplitude,period/2);level(d,140,period-period/2);}
}
static void finish(D &d){level(d,140,1600000);}
static void finalMark(D &d){level(d,240,1700000);}
static void broadMarks(D &d,uint32_t period,unsigned count) {
  // One broad reflective mark, with a 30-count subsidiary dip on its plateau.
  // The known fundamental is the independent timing oracle, not the code's
  // detected edges. A smaller hysteresis can count the internal dip as a cycle.
  for(unsigned cycle=0;cycle<count;++cycle)for(uint32_t us=0;us<period;us+=20) {
    const float t=float(us)/period;
    float value=140;
    if(t<.1f)value=140+100*t/.1f;
    else if(t<.2f)value=240;
    else if(t<.25f)value=240-30*(t-.2f)/.05f;
    else if(t<.35f)value=210+30*(t-.25f)/.1f;
    else if(t<.5f)value=240;
    else if(t<.6f)value=240-100*(t-.5f)/.1f;
    d.feed(uint16_t(value));
  }
}
int main() {
  D defaults;assert(defaults.minMark==300 && defaults.edgeSwingFraction==.30f);
  assert(!defaults.singleTurnPeak);
  for(uint32_t period:{4000u,8000u,10000u,16000u,40000u,59000u}) {
    D d=ready();square(d,period,12);finish(d);
    assert(d.launches==1 && std::fabs(d.resultRpm-60000000.0f/period)<3);
  }
  for(uint32_t period:{61000u,100000u,200000u}) {
    D d=ready();square(d,period,12);finish(d);assert(d.launches==0);
  }
  for(int amplitude:{15,30,45,int(StickS3SensorProfile::MIN_MARK)-1}) {
    D d=ready();square(d,10000,16,amplitude);finish(d);
    assert(d.launches==0 && d.weakWindows>0);
  }
  // The TCRT trial's measured marks are about 63-71 counts. Exercise complete
  // weak-but-valid pulse trains against an independent period oracle, while
  // the unchanged standard profile must continue rejecting the same marks.
  for(int amplitude:{63,65,71}) {
    D d=ready();square(d,12000,12,amplitude);finish(d);
    if(StickS3SensorProfile::MIN_MARK<=60)
      assert(d.launches==1 && std::fabs(d.resultRpm-5000)<3);
    else assert(d.launches==0);
  }
  for(uint32_t period:{8000u,12000u,20000u}) {
    D d=ready();broadMarks(d,period,18);finish(d);
    assert(d.launches==1 && std::fabs(d.resultRpm-60000000.0f/period)<3);
  }
  D shortBurst=ready();square(shortBurst,10000,2);finish(shortBurst);assert(shortBurst.launches==0);
  // Established-pull context supports even one clean final fast turn. The last
  // rising mark completes it; no additional full fast turn is needed.
  for(unsigned n:{1u,2u,3u}) {
    D tail=ready();square(tail,10000,12);square(tail,4000,n);finalMark(tail);
    assert(tail.singleTurnPeak && tail.launches==1 && tail.resultValid);
    assert(std::fabs(tail.resultRpm-15000)<3 && std::fabs(tail.sustainedPeakRpm-(n<3?6000:15000))<3);
  }
  D accelerating=ready();square(accelerating,10000,12);
  for(unsigned us:{10000u,9600u,9200u,8800u,8400u,8000u,7600u,7200u,6800u,6400u,6000u})square(accelerating,us,1);
  finalMark(accelerating);assert(accelerating.launches==1);
  assert(std::fabs(accelerating.resultRpm-10000)<3 && std::fabs(accelerating.sustainedPeakRpm-9375)<3);
  // A short surge followed by normal speed must not end the outward pull.
  D middle=ready();square(middle,10000,12);square(middle,6000,1);square(middle,10000,8);square(middle,4000,8);finalMark(middle);
  assert(middle.launches==1 && std::fabs(middle.resultRpm-15000)<3);
  // Every turn still needs valid pulse width, strong matching contrast and
  // matching optical duty, including its immediately preceding mark.
  for(int amplitude:{30,300}) {
    D bad=ready();square(bad,10000,12);square(bad,4000,1,amplitude);square(bad,10000,8);finish(bad);
    assert(bad.launches==1 && std::fabs(bad.resultRpm-6000)<3);
  }
  D badDuty=ready();square(badDuty,10000,12);level(badDuty,240,3600);level(badDuty,140,400);square(badDuty,10000,8);finish(badDuty);
  assert(badDuty.launches==1 && std::fabs(badDuty.resultRpm-6000)<3 && badDuty.singlePeakRejected>0);
  D narrow=ready();square(narrow,10000,12);level(narrow,240,100);level(narrow,140,100);square(narrow,10000,8);finish(narrow);
  assert(narrow.launches==1 && narrow.resultRpm<6100);
  D tooFast=ready();square(tooFast,10000,12);square(tooFast,1000,1);square(tooFast,10000,8);finish(tooFast);
  assert(tooFast.launches==1 && tooFast.resultRpm<6100);
  D maximum=ready();square(maximum,10000,12);square(maximum,2000,1);finalMark(maximum);
  assert(maximum.launches==1 && std::fabs(maximum.resultRpm-30000)<3);
  D rewind=ready();square(rewind,10000,12);level(rewind,140,60000);
  square(rewind,15000,80);finish(rewind);assert(rewind.launches==1);
  square(rewind,8000,12);finish(rewind);assert(rewind.launches==2);
  D loss=ready();square(loss,10000,3);loss.dataLoss();square(loss,10000,12);
  finish(loss);assert(loss.launches==0);
  D paused=ready();paused.setEnabled(false);square(paused,10000,12);finish(paused);assert(paused.launches==0);
  // Persistent startup 60 Hz interference must not arm, and a legitimate
  // 60 Hz pull after quiet must still be measurable. There is no 60 Hz notch.
  D mains;StickS3SensorProfile::configure(mains);square(mains,16680,180,1000);
  assert(mains.launches==0 && mains.phase==D::Phase::Settling);
  D sixty=ready();square(sixty,16680,12);finish(sixty);
  assert(sixty.launches==1 && std::fabs(sixty.sustainedPeakRpm-60000000.0f/16680)<3);
  // Single-turn edge times are quantized to the independent 100-us detector
  // cadence. Preserve the tighter three-turn oracle and bound the new peak by
  // at most one tick of period error; this is not an extra physical turn.
  const float trueSixty=60000000.0f/16680;
  assert(sixty.resultRpm>=trueSixty-3 && sixty.resultRpm<=60000000.0f/(16680-100));
  // Ring ordering across wrap and exports after data loss. Only the newest
  // contiguous segment may be exported; the hot path needs no 64-bit modulo.
  uint16_t storage[7]={};SampleRing ring(storage,7);
  for(uint16_t i=0;i<20;++i)ring.record(i);
  assert(ring.count()==7 && ring.first()==13 && ring.total()==20);
  for(unsigned i=0;i<7;++i)assert(storage[(ring.first()+i)%7]==13+i);
  ring.discontinuity();assert(ring.count()==0);
  for(uint16_t i=20;i<23;++i)ring.record(i);
  assert(ring.count()==3 && ring.first()==20);
  for(unsigned i=0;i<3;++i)assert(storage[(ring.first()+i)%7]==20+i);
  SampleRing noStorage(nullptr,7);noStorage.record(5);assert(noStorage.count()==0);
  std::cout<<"PASS: M5 single-turn final peaks, accelerating tails, continued pull after a brief surge, malformed/weak/contrast/duty rejection, 30000-RPM timing boundary, period oracle, multi-peak marks, profile mark floor, 1000-RPM floor, short burst, rewind, pause/data loss, 60-Hz handling and recorder wrap/discontinuity\n";
}
