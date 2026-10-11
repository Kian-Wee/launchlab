// Public-API regression with a generated optical timing oracle, not measured
// shaft-RPM calibration. Each 500-sample stripe cycle is exactly 10 ms at
// 50 kS/s (6000 RPM). A subsidiary dip is shorter and smaller than its full
// 1000-count stripe mark; the Standard profile's 0.7 swing hysteresis should
// suppress it. Prior completed-ring position must not alter these same inputs.
#include "../LaunchLabMini/sensor_profile.h"
#include "../LaunchLabMini/history_checkpoint.h"
#include "../LaunchLabMini/rpm_estimator.h"
#include <cmath>
#include <iostream>
#include <stdint.h>

struct Trace {
  AnalogTachometer d;
  uint64_t stageOrigin=0,firstConfirmationUs=0;
  Trace(){StickS3SensorProfile::configure(d,StickS3SensorProfile::Mode::Standard);}
  void sample(uint16_t value) {
    const auto before=d.phase;d.feed(value);
    if(stageOrigin && !firstConfirmationUs && before==AnalogTachometer::Phase::Ready && d.phase==AnalogTachometer::Phase::Launch)
      firstConfirmationUs=d.nowUs-stageOrigin;
  }
  void constant(unsigned us,uint16_t value){for(unsigned i=0;i<us/20;++i)sample(value);}
  void cycles(unsigned count,bool narrowLow,unsigned notchDepth) {
    for(unsigned cycle=0;cycle<count;++cycle)for(unsigned i=0;i<500;++i) {
      unsigned value=i<(narrowLow?50u:250u)?1000:2000;
      const unsigned notchStart=narrowLow?250u:333u;
      if(notchDepth && i>=notchStart && i<notchStart+50)value-=notchDepth;
      sample(uint16_t(value));
    }
  }
  bool prefix(unsigned count,bool checkpoint) {
    constant(1100000,1000);cycles(count,false,0);constant(1200000,1000);
    if(d.launches!=1 || !d.resultValid || !HistoryCheckpoint::allowedIdle(d,d.launches))return false;
    if(checkpoint){const HistoryCheckpoint saved(d);saved.restore(d);}
    return true;
  }
};
static bool generatedRpm(float rpm) {
  // Feed evaluates every 5 samples (100 us). Allow a conservative one-grid
  // timing error per period in either selected estimator, independently of
  // detector output and of the frozen implementation's wrong results.
  return std::isfinite(rpm) && rpm>=60000000.0f/10100-0.01f && rpm<=60000000.0f/9900+0.01f;
}
struct Outcome {
  unsigned launches=0;
  float three=0,single=0;
  uint64_t confirmedUs=0,startUs=0,endUs=0;
  bool valid=false,gap=false,ready=false;
  bool pass=false;
};
static Outcome burst(Trace &t,unsigned turns,bool narrowLow,unsigned depth) {
  const unsigned launches=t.d.launches;
  t.stageOrigin=t.d.nowUs;t.firstConfirmationUs=0;
  t.cycles(turns,narrowLow,depth);t.constant(1200000,1000);
  Outcome o;
  o.launches=t.d.launches-launches;o.three=t.d.sustainedPeakRpm;o.single=t.d.singlePeakRpm;
  o.confirmedUs=t.firstConfirmationUs;
  o.startUs=t.d.burstStartUs>=t.stageOrigin?t.d.burstStartUs-t.stageOrigin:0;
  o.endUs=t.d.burstEndUs>=t.stageOrigin?t.d.burstEndUs-t.stageOrigin:0;
  o.valid=t.d.resultValid;o.gap=t.d.ended==AnalogTachometer::End::Gap;o.ready=t.d.phase==AnalogTachometer::Phase::Ready;
  // All accepted marks lie in the generated burst, whose speed never slows.
  // The subsequent constant signal must end via Gap and recover to Ready.
  const bool boundaries=o.confirmedUs>0 && o.confirmedUs<=uint64_t(turns)*10000+1000 &&
    o.startUs<o.endUs && o.endUs<=uint64_t(turns)*10000+1000;
  o.pass=o.launches==1 && o.valid && o.gap && o.ready && boundaries &&
    generatedRpm(RpmEstimator::select(RpmEstimator::Mode::ThreeTurn,o.three,o.single)) &&
    generatedRpm(RpmEstimator::select(RpmEstimator::Mode::SingleTurn,o.three,o.single));
  return o;
}
static void report(const char *kind,unsigned prefix,bool checkpoint,unsigned turns,unsigned depth,const Outcome &o,const Outcome &repeat) {
  std::cout<<"{\"case\":\""<<kind<<"\",\"prefix_cycles\":"<<prefix<<",\"checkpoint\":"<<(checkpoint?"true":"false")
    <<",\"tail_cycles\":"<<turns<<",\"subsidiary_depth\":"<<depth<<",\"generated_period_us\":10000"
    <<",\"expected_launches\":1,\"observed_launches\":"<<o.launches<<",\"three_rpm\":"<<o.three<<",\"single_rpm\":"<<o.single
    <<",\"first_confirmation_us\":"<<o.confirmedUs<<",\"burst_start_us\":"<<o.startUs<<",\"burst_end_us\":"<<o.endUs
    <<",\"gap_end\":"<<(o.gap?"true":"false")<<",\"ready_after_quiet\":"<<(o.ready?"true":"false")
    <<",\"first_pass\":"<<(o.pass?"true":"false")<<",\"repeat_launches\":"<<repeat.launches
    <<",\"repeat_three_rpm\":"<<repeat.three<<",\"repeat_single_rpm\":"<<repeat.single
    <<",\"repeat_confirmation_us\":"<<repeat.confirmedUs<<",\"repeat_pass\":"<<(repeat.pass?"true":"false")
    <<",\"pass\":"<<(o.pass && repeat.pass?"true":"false")<<"}\n";
}
int main() {
  unsigned failed=0;
  for(unsigned prefix:{5u,6u,7u,8u})for(bool checkpoint:{false,true})for(unsigned tail:{5u,20u}) {
    Trace t;if(!t.prefix(prefix,checkpoint))return 2;
    const auto first=burst(t,tail,false,300),repeat=burst(t,5,false,300);
    report("short_burst",prefix,checkpoint,tail,300,first,repeat);
    if(!first.pass || !repeat.pass)++failed;
  }
  // Exactly 24 complementary low-duty/high-duty morphology cases. Known
  // identical 10-ms fundamentals must remain near 6000 under both selectors,
  // including prior ring-index 5/7 failure cases and prefix-6/no-reset controls.
  for(unsigned prefix:{5u,6u,7u})for(bool checkpoint:{false,true})for(unsigned depth:{300u,400u,500u,600u}) {
    Trace t;if(!t.prefix(prefix,checkpoint))return 2;
    const auto first=burst(t,8,true,depth),repeat=burst(t,8,true,depth);
    report("subsidiary_high",prefix,checkpoint,8,depth,first,repeat);
    if(!first.pass || !repeat.pass)++failed;
  }
  return failed?1:0;
}
