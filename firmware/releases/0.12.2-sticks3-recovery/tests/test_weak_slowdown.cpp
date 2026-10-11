#include "../LaunchLabMini/sensor_profile.h"
#include "../LaunchLabMini/rpm_estimator.h"
#include <cassert>
#include <cmath>
#include <iostream>
static void level(AnalogTachometer& d,unsigned us,int value){for(unsigned i=0;i<us/20;++i)d.feed(value);}
static void square(AnalogTachometer& d,unsigned period,unsigned turns,int amplitude=100){
  for(unsigned n=0;n<turns;++n){level(d,period/2,140+amplitude);level(d,period-period/2,140);}
}
int main(){
  const int floor=int(StickS3SensorProfile::MIN_MARK),strongAmplitude=floor*5/4;
  for(int amplitude:{floor*9/10,floor*19/20,floor-1}){
    AnalogTachometer d;StickS3SensorProfile::configure(d);level(d,1200000,140);
    square(d,10000,12,strongAmplitude);assert(d.phase==AnalogTachometer::Phase::Launch);
    // Marks exceed reversal hysteresis but fall below the selected profile's
    // validity floor. They cannot prove rewind.
    square(d,18000,2,amplitude);square(d,5000,12,strongAmplitude);
    level(d,1700000,140);
    assert(d.launches==1 && d.resultValid);
    assert(d.ended==AnalogTachometer::End::Gap);
    for(auto mode:{RpmEstimator::Mode::ThreeTurn,RpmEstimator::Mode::SingleTurn})
      assert(std::fabs(RpmEstimator::select(mode,d.sustainedPeakRpm,d.singlePeakRpm)-12000)<3);
  }
  // Real strong slowdown continues to close the original burst, excluding
  // following turns until the one-second quiet/rearm interval completes.
  AnalogTachometer strong;StickS3SensorProfile::configure(strong);level(strong,1200000,140);
  square(strong,10000,12);square(strong,18000,4);square(strong,5000,12);level(strong,1700000,140);
  assert(strong.launches==1 && strong.ended==AnalogTachometer::End::Slowdown);
  assert(std::fabs(strong.sustainedPeakRpm-6000)<3);
  std::cout<<"PASS: subthreshold slowdown marks cannot discard a later peak in either RPM mode; strong slowdown/rearm retained\n";
}
