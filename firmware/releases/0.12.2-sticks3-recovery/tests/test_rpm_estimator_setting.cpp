#include "../LaunchLabMini/rpm_estimator_setting.h"
#include "../LaunchLabMini/practice_store.h"
#include <cassert>
#include <iostream>
int main() {
  Preferences prefs;prefs.begin("launchlab-mini",false);
  const uint8_t timeout=7,wiring=1;const uint8_t opaque[]={3,4,5};
  prefs.putBytes("sleep_min",&timeout,1);prefs.putBytes("ao_pin",&wiring,1);
  prefs.putBytes("wake_ui",opaque,3);
  PracticeHistory history;PracticeStore store;assert(store.begin(history));
  assert(history.accept(1,true,6000,10) && store.save(history));
  const auto before=Preferences::data;
  RpmEstimator::Mode mode=RpmEstimator::Mode::SingleTurn;
  assert(RpmEstimatorSetting::load(prefs,mode) && mode==RpmEstimator::DEFAULT_MODE);
  assert(Preferences::data==before); // Migration is read-only; no old keys reset.
  assert(RpmEstimatorSetting::save(prefs,RpmEstimator::Mode::SingleTurn));
  Preferences reboot;reboot.begin("launchlab-mini",false);
  assert(RpmEstimatorSetting::load(reboot,mode) && mode==RpmEstimator::Mode::SingleTurn);
  for(const auto &entry:before)assert(Preferences::data.at(entry.first)==entry.second);
  Preferences::failWrite=true;const auto saved=Preferences::data;
  assert(!RpmEstimatorSetting::save(prefs,RpmEstimator::Mode::ThreeTurn));
  assert(Preferences::data==saved);Preferences::failWrite=false;
  assert(RpmEstimatorSetting::save(prefs,RpmEstimator::Mode::ThreeTurn));
  assert(RpmEstimatorSetting::load(reboot,mode) && mode==RpmEstimator::Mode::ThreeTurn);
  history.newSession();assert(history.accept(2,true,7000,20));
  assert(history.sessions()==2 && history.session()->mean()==7000 && history.session(1)->mean()==6000);
  auto &record=Preferences::data["launchlab-minirpm_method"];record[2]=7;
  const auto corrupt=Preferences::data;
  assert(!RpmEstimatorSetting::load(reboot,mode) && mode==RpmEstimator::DEFAULT_MODE);
  assert(Preferences::data==corrupt); // Retain invalid bytes for recovery.
  assert(!RpmEstimatorSetting::save(prefs,RpmEstimator::Mode(7)) && Preferences::data==corrupt);
  std::cout<<"PASS: three-turn default migration without writes, both choices persist, existing history/settings retained, failed write and corrupt record retention, new-session separation\n";
}
