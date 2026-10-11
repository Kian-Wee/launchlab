// Generated optical references, not calibrated shaft RPM. Four raw rising
// markers enclose three complete periods. A rejected latest arrival cannot
// erase two already qualified predecessor peaks or their paired amplitudes.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
// Review-only: compile against an independently pinned reset-only copy too.
// The ordinary permanent suite needs only the public generator oracles below.
#ifdef STARTUP_BASELINE_HEADER
namespace ResetOnly {
#include STARTUP_BASELINE_HEADER
}
#endif
#include "../LaunchLabMini/sensor_profile.h"
#include "../LaunchLabMini/rpm_estimator.h"
#include "../LaunchLabMini/practice_history.h"

struct Completed {float rpm=0,amplitude=0;};
struct Trace {
  AnalogTachometer d;
  PracticeHistory history;
  RpmEstimator::Mode mode;
  uint16_t prior=1000;
  uint32_t revision=0;
  uint64_t checkedSamples=0,confirmation=0,arrivalDecision=0;
  uint32_t startingCycles=0;
  float seedRpm=0,seedAmplitude=0;
  bool recording=false;
  std::vector<uint64_t> references;
  std::vector<Completed> completed;
#ifdef STARTUP_BASELINE_HEADER
  ResetOnly::AnalogTachometer baseline;
  PracticeHistory baselineHistory;
  uint32_t baselineRevision=0;
#endif
  explicit Trace(RpmEstimator::Mode m):mode(m) {
    StickS3SensorProfile::configure(d);
    assert(!d.singleTurnPeak && d.trackSingleTurnPeak);
#ifdef STARTUP_BASELINE_HEADER
    baseline.minMark=StickS3SensorProfile::MIN_MARK;
    baseline.edgeSwingFraction=StickS3SensorProfile::EDGE_SWING_FRACTION;
    baseline.singleTurnPeak=false;baseline.trackSingleTurnPeak=true;
#endif
  }
  template<class Detector> static void publish(const Detector &a,RpmEstimator::Mode m,
                                               PracticeHistory &h,uint32_t &seen) {
    if(a.resultRevision==seen)return;
    seen=a.resultRevision;
    const float selected=a.resultValid?RpmEstimator::select(m,a.sustainedPeakRpm,a.singlePeakRpm):0;
    // Same validity/selection contract as acquisition.publish and UI history.
    h.accept(a.launches,a.resultValid && selected>0,selected,uint32_t(a.nowUs/1000));
  }
  void sample(uint16_t value) {
    const auto phase=d.phase;const uint32_t edges=d.opticalEdges;
    if(recording && value>prior)references.push_back(d.nowUs+20);
    prior=value;d.feed(value);++checkedSamples;
    if(recording && d.opticalEdges!=edges && d.signalContrast>=StickS3SensorProfile::MIN_MARK)
      completed.push_back({d.candidateRpm,d.signalContrast});
    if(recording && phase==AnalogTachometer::Phase::Ready && d.phase==AnalogTachometer::Phase::Launch) {
      assert(!confirmation);confirmation=d.nowUs;startingCycles=d.cycles;
      seedRpm=d.singlePeakRpm;seedAmplitude=d.singlePeakAmplitude;
    }
    if(recording && confirmation && !arrivalDecision && completed.size()==3 &&
       completed[2].rpm>std::max(completed[0].rpm,completed[1].rpm) &&
       (d.singlePeakRejected || d.singlePeakRpm==completed[2].rpm))arrivalDecision=d.nowUs;
    publish(d,mode,history,revision);
#ifdef STARTUP_BASELINE_HEADER
    baseline.feed(value);publish(baseline,mode,baselineHistory,baselineRevision);
    // Only alternative single peak/paired amplitude/update count may change.
    // State, three-turn result, rejection counters and every boundary stay exact.
#define SAME(field) assert(d.field==baseline.field)
    assert(int(d.phase)==int(baseline.phase) && int(d.ended)==int(baseline.ended));
    SAME(enabled);SAME(resultValid);SAME(signalFault);SAME(peakRpm);SAME(resultRpm);
    SAME(candidateRpm);SAME(sustainedPeakRpm);SAME(sustainedPeakAmplitude);SAME(peakAmplitude);
    SAME(singlePeakRejected);SAME(startNoiseBand);SAME(signalLow);SAME(signalHigh);
    SAME(signalContrast);SAME(noiseBand);SAME(hysteresis);SAME(launches);SAME(attempts);
    SAME(resultRevision);SAME(cycles);SAME(revolutions);SAME(opticalEdges);SAME(rejectedPeriods);
    SAME(rejectedShape);SAME(inconsistentWindows);SAME(slowWindows);SAME(weakWindows);
    SAME(rearmExtensions);SAME(nowUs);SAME(resultUs);SAME(burstStartUs);SAME(burstEndUs);
    assert(d.quietMs()==baseline.quietMs() && d.rearmMs()==baseline.rearmMs());
    assert(d.historyCheckpointIdle()==baseline.historyCheckpointIdle());
#undef SAME
#endif
  }
  void level(uint16_t value,unsigned us) {assert(us%20==0);for(unsigned n=0;n<us/20;++n)sample(value);}
  void start(unsigned weak) {
    level(1000,1100000);assert(d.phase==AnalogTachometer::Phase::Ready && !d.launches);
    // Shape-valid 30-count prehistory is below the80-count profile floor.
    // It advances the real period ring without private-array manipulation.
    for(unsigned n=0;n<weak;++n){level(1030,5000);level(1000,5000);}
    assert(d.phase==AnalogTachometer::Phase::Ready && !d.launches && prior==1000);
  }
  void beginRecording() {references.clear();completed.clear();confirmation=arrivalDecision=0;startingCycles=0;recording=true;assert(prior==1000);}
};
static bool rawPeriodBound(float rpm,unsigned period) {
  // The 25-sample mean spans500us and optical evaluation has100us cadence.
  // Adaptive threshold crossing can shift an extracted interval relative to
  // raw references by up to600us. Exact predecessor lineage is checked too.
  return std::isfinite(rpm) && rpm>=60000000.0f/(period+600)-.01f && rpm<=60000000.0f/(period-600)+.01f;
}
static bool boundaries(const Trace &t) {
  return t.confirmation>=t.references[3] && t.confirmation<=t.references[3]+600 &&
    int64_t(t.d.burstStartUs)>=int64_t(t.references.front())-600 && t.d.burstStartUs<=t.references.front()+600 &&
    int64_t(t.d.burstEndUs)>=int64_t(t.references.back())-600 && t.d.burstEndUs<=t.references.back()+600;
}
static bool first(Trace &t,unsigned weak,bool earlier,bool oversize,unsigned middleAmplitude) {
  t.start(weak);t.beginRecording();const uint64_t origin=t.d.nowUs;
  const std::array<unsigned,3> periods=earlier?std::array<unsigned,3>{8400,9000,10000}:std::array<unsigned,3>{10000,9000,8400};
  const std::array<unsigned,3> amplitudes={1000,middleAmplitude,1000};
  for(unsigned n=0;n<3;++n){t.level(uint16_t(1000+amplitudes[n]),periods[n]/2);t.level(1000,periods[n]/2);}
  t.level(oversize?4000:2000,3000);
  // No fifth marker or Gap publication is needed to resolve this arrival.
  const float resolvedRpm=t.d.singlePeakRpm,resolvedAmplitude=t.d.singlePeakAmplitude;
  t.level(1000,1200000);t.recording=false;
  assert((t.references==std::vector<uint64_t>{origin+20,origin+periods[0]+20,origin+periods[0]+periods[1]+20,origin+27400+20}));
  assert(t.completed.size()==3 && t.startingCycles==weak+3);
  for(unsigned n=0;n<3;++n){assert(rawPeriodBound(t.completed[n].rpm,periods[n]));assert(t.completed[n].amplitude==amplitudes[n]);}
  const Completed predecessor=t.completed[0].rpm>t.completed[1].rpm?t.completed[0]:t.completed[1];
  // The latest turn is uniquely fastest for the affected startup branch.
  if(!earlier)assert(t.completed[2].rpm>predecessor.rpm);
  const Completed expected=earlier?t.completed[0]:(oversize?predecessor:t.completed[2]);
  const unsigned measuredSpan=unsigned(std::lround(60000000.0f/t.completed[0].rpm))+unsigned(std::lround(60000000.0f/t.completed[1].rpm))+unsigned(std::lround(60000000.0f/t.completed[2].rpm));
  const float three=180000000.0f/measuredSpan;
  const bool seed=earlier || !oversize || (t.seedRpm==predecessor.rpm && t.seedAmplitude==predecessor.amplitude);
  const float selected=RpmEstimator::select(t.mode,t.d.sustainedPeakRpm,t.d.singlePeakRpm);
  const bool peak=t.d.singlePeakRpm==expected.rpm && t.d.singlePeakAmplitude==expected.amplitude;
  const bool arrival=resolvedRpm==expected.rpm && resolvedAmplitude==expected.amplitude &&
    (earlier || t.arrivalDecision==t.confirmation+500);
  const bool guard=t.d.singlePeakRejected==(oversize?1u:0u);
  const bool saved=t.history.size()==1 && t.history.recent() && t.history.recent()->rpm==selected;
  const bool passed=seed && peak && arrival && guard && saved && t.d.launches==1 && t.d.attempts==1 && t.d.resultRevision==1 &&
    t.d.resultValid && t.d.phase==AnalogTachometer::Phase::Ready && t.d.ended==AnalogTachometer::End::Gap && boundaries(t) &&
    std::fabs(t.d.sustainedPeakRpm-three)<.01f;
  std::cout<<"{\"kind\":\"startup\",\"weak_periods\":"<<weak<<",\"latest_slot\":"<<(t.startingCycles-1)%3
    <<",\"earlier_fastest\":"<<(earlier?"true":"false")<<",\"oversize_arrival\":"<<(oversize?"true":"false")
    <<",\"middle_amplitude\":"<<middleAmplitude<<",\"estimator\":\""<<RpmEstimator::name(t.mode)<<"\",\"raw_references_us\":[";
  for(unsigned n=0;n<4;++n)std::cout<<(n?",":"")<<t.references[n];
  std::cout<<"],\"confirmation_us\":"<<t.confirmation<<",\"arrival_decision_us\":"<<t.arrivalDecision<<",\"starting_cycles\":"<<t.startingCycles<<",\"seed_rpm\":"<<t.seedRpm
    <<",\"seed_amplitude\":"<<t.seedAmplitude<<",\"predecessor_rpm\":"<<predecessor.rpm<<",\"predecessor_amplitude\":"<<predecessor.amplitude
    <<",\"latest_rpm\":"<<t.completed[2].rpm<<",\"three_rpm\":"<<t.d.sustainedPeakRpm<<",\"single_rpm\":"<<t.d.singlePeakRpm
    <<",\"single_amplitude\":"<<t.d.singlePeakAmplitude<<",\"single_rejected\":"<<t.d.singlePeakRejected<<",\"selected_rpm\":"<<selected
    <<",\"history_size\":"<<t.history.size()<<",\"pass\":"<<(passed?"true":"false");
#ifdef STARTUP_BASELINE_HEADER
  std::cout<<",\"baseline_single_rpm\":"<<t.baseline.singlePeakRpm<<",\"baseline_history_size\":"<<t.baselineHistory.size();
  if(earlier){assert(t.d.singlePeakRpm==t.baseline.singlePeakRpm && t.d.singlePeakAmplitude==t.baseline.singlePeakAmplitude && t.d.singlePeakUpdates==t.baseline.singlePeakUpdates);}
#endif
  std::cout<<"}\n";return passed;
}
static bool next(Trace &t) {
  const uint32_t oldCount=t.d.launches,oldRevision=t.d.resultRevision,oldHistory=t.history.size();
  assert(t.d.phase==AnalogTachometer::Phase::Ready);t.beginRecording();const uint64_t origin=t.d.nowUs;
  for(unsigned n=0;n<8;++n){t.level(2000,5000);t.level(1000,5000);}t.level(1000,1200000);t.recording=false;
  assert(t.references.size()==8);for(unsigned n=0;n<8;++n)assert(t.references[n]==origin+10000*n+20);
  const float selected=RpmEstimator::select(t.mode,t.d.sustainedPeakRpm,t.d.singlePeakRpm);
  const bool pass=t.d.launches==oldCount+1 && t.d.resultRevision==oldRevision+1 && t.history.size()==oldHistory+1 &&
    t.d.resultValid && t.d.phase==AnalogTachometer::Phase::Ready && t.d.ended==AnalogTachometer::End::Gap && boundaries(t) &&
    rawPeriodBound(t.d.sustainedPeakRpm,10000) && rawPeriodBound(t.d.singlePeakRpm,10000) && t.history.recent()->rpm==selected;
  std::cout<<"{\"kind\":\"subsequent_no_reset\",\"estimator\":\""<<RpmEstimator::name(t.mode)<<"\",\"fourth_raw_reference_us\":"<<t.references[3]
    <<",\"confirmation_us\":"<<t.confirmation<<",\"launches\":"<<t.d.launches<<",\"history_size\":"<<t.history.size()
    <<",\"selected_rpm\":"<<selected<<",\"checked_samples\":"<<t.checkedSamples<<",\"pass\":"<<(pass?"true":"false")<<"}\n";
  return pass;
}
int main() {
  std::cout<<std::setprecision(9);unsigned failures=0,cases=0;
  for(unsigned weak:{0u,1u,2u})for(unsigned amplitude:{1000u,1200u})for(auto mode:{RpmEstimator::Mode::ThreeTurn,RpmEstimator::Mode::SingleTurn})
    for(unsigned kind=0;kind<3;++kind){Trace t(mode);const bool initial=first(t,weak,kind==2,kind==1,amplitude),repeat=next(t);++cases;if(!initial || !repeat)++failures;}
  std::cout<<"{\"summary\":true,\"cases\":"<<cases<<",\"bursts\":"<<cases*2<<",\"failed_cases\":"<<failures<<"}\n";
  return failures?1:0;
}
