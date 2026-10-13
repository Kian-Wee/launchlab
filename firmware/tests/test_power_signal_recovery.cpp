#include "../LaunchLabMini/sensor_profile.h"
#include "../LaunchLabMini/rpm_estimator.h"
#include <cassert>
#include <cmath>
#include <iostream>

using D=AnalogTachometer;
static void level(D &d,unsigned us,int value=1000) {
  assert(us%20==0);for(unsigned i=0;i<us/20;++i)d.feed(value);
}
static void square(D &d,unsigned period,unsigned turns,int amplitude) {
  for(unsigned i=0;i<turns;++i){level(d,period/2,1000+amplitude);level(d,period-period/2);}
}
static D ready(StickS3SensorProfile::Mode profile) {
  D d;StickS3SensorProfile::configure(d,profile);level(d,1200000);
  assert(d.phase==D::Phase::Ready);return d;
}
static void peaks(const D &d,float rpm) {
  assert(d.resultValid);
  for(auto mode:{RpmEstimator::Mode::ThreeTurn,RpmEstimator::Mode::SingleTurn})
    assert(std::fabs(RpmEstimator::select(mode,d.sustainedPeakRpm,d.singlePeakRpm)-rpm)<3);
}
int main() {
  static_assert(D::MIN_LAUNCH_RPM==1000 && D::QUIET_US==1000000);
  for(auto profile:{StickS3SensorProfile::Mode::Standard,StickS3SensorProfile::Mode::Tcrt}) {
    D startup;StickS3SensorProfile::configure(startup,profile);
    // Periodic weak background persists throughout startup, rather than first
    // giving the detector flat input. It must arm without inventing a launch.
    square(startup,10000,1000,25);
    assert(startup.phase==D::Phase::Ready && !startup.launches && startup.weakWindows>0);

    D rearm=ready(profile);square(rearm,10000,12,100);level(rearm,60000);
    assert(rearm.phase==D::Phase::Hold && rearm.launches==1);
    square(rearm,10000,120,25);
    assert(rearm.phase==D::Phase::Ready && rearm.launches==1);peaks(rearm,6000);

    // A gradual decline preserves edge tracking, unlike an abrupt amplitude
    // drop that can close through the existing gap rule before noise resumes.
    D tail=ready(profile);square(tail,10000,12,100);
    for(int amplitude=100;amplitude>=25;--amplitude)square(tail,10000,1,amplitude);
    const uint64_t weakBegin=tail.nowUs;
    square(tail,10000,1000,25);
    assert(tail.phase==D::Phase::Ready && tail.launches==1 && tail.ended==D::End::Gap);
    assert(tail.weakWindows>0 && tail.burstEndUs<=weakBegin);
    assert(tail.resultUs<=weakBegin+D::QUIET_US+10000);peaks(tail,6000);
    const uint32_t revision=tail.resultRevision;
    square(tail,10000,200,25);
    assert(tail.resultRevision==revision && tail.launches==1);peaks(tail,6000);

    // A short loss of contrast is still part of the same pull. Its later
    // strong acceleration must remain available to both peak estimators.
    D recovery=ready(profile);square(recovery,10000,12,100);
    for(int amplitude=100;amplitude>=25;--amplitude)square(recovery,10000,1,amplitude);
    square(recovery,10000,10,25);
    assert(recovery.phase==D::Phase::Launch && !recovery.launches);
    square(recovery,5000,12,100);level(recovery,1700000);
    assert(recovery.launches==1 && recovery.ended==D::End::Gap);peaks(recovery,12000);

    // Strong slowdown/rewind closes the original burst and keeps rearm held
    // until one full second after genuine motion, even well below 1000 RPM.
    D rewind=ready(profile);square(rewind,10000,12,100);square(rewind,18000,4,100);
    assert(rewind.phase==D::Phase::Hold && rewind.ended==D::End::Slowdown);
    square(rewind,150000,12,100);
    assert(rewind.phase==D::Phase::Hold && rewind.launches==1 && rewind.rearmExtensions>0);
    const unsigned remaining=rewind.rearmMs();assert(remaining>0);
    level(rewind,(remaining-1)*1000);assert(rewind.phase==D::Phase::Hold);
    level(rewind,2000);assert(rewind.phase==D::Phase::Ready);peaks(rewind,6000);
    square(rewind,61000,12,100);level(rewind,1200000);
    assert(rewind.launches==1); // Launch floor unchanged.
  }
  // Archived earlier-case optics deliberately disabled amplitude validation.
  // Keep its startup motion fence and continuous weak rotation behavior.
  D legacy;legacy.minMark=0;square(legacy,10000,200,25);
  assert(legacy.phase==D::Phase::Settling && !legacy.launches);
  level(legacy,1200000);assert(legacy.phase==D::Phase::Ready);
  square(legacy,10000,1200,25);
  assert(legacy.phase==D::Phase::Launch && !legacy.launches);
  level(legacy,1200000);assert(legacy.launches==1 && legacy.phase==D::Phase::Ready);
  std::cout<<"PASS: both sensor profiles arm/rearm through weak noise, bound tapered weak tails, retain both peaks and transient recovery, preserve true slow rewind, 1000 RPM floor, one-second quiet and minMark=0 optics\n";
}
