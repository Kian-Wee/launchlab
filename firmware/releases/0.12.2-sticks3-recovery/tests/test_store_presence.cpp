#include "../LaunchLabMini/practice_store.h"
#include "../LaunchLabMini/motion_store.h"
#include "../LaunchLabMini/rpm_estimator_setting.h"
#include <cassert>
#include <iostream>

int main() {
  // A key can exist while getBytesLength() returns zero, including a wrong NVS
  // type. The installed ESP32 Preferences implementation returns zero for all
  // nvs_get_blob length-query errors; a zero-length value is not proof of absence.
  Preferences::data.clear();Preferences::zeroLengthKeys.clear();
  Preferences::data["ll-practicehistory1"]={1,2,3,4};
  Preferences::zeroLengthKeys.insert("ll-practicehistory1");
  const auto historyBytes=Preferences::data;
  PracticeHistory history;PracticeStore practice;
  const bool historyOpened=practice.begin(history);
  history.accept(1,true,6000,1000);
  const bool historySaved=practice.save(history);
  const bool historyRetained=Preferences::data==historyBytes;
  std::cout<<"history existing key with failed length query: opened="<<historyOpened<<" saved="<<historySaved
           <<" retained="<<historyRetained<<"\n";

  Preferences::data.clear();Preferences::zeroLengthKeys.clear();
  Preferences::data["ll-motionref1"]={5,6,7,8};
  Preferences::zeroLengthKeys.insert("ll-motionref1");
  const auto referenceBytes=Preferences::data;
  LaunchMotion::Trace reference;MotionStore motion;
  const bool referenceOpened=motion.begin(reference);
  LaunchMotion::Trace t;t.fused=true;t.quality=LaunchMotion::Quality::Valid;
  t.count=48;t.number=1;t.rpm=5000;t.durationMs=400;t.gravity[2]=1;
  for(unsigned i=0;i<t.count;++i)t.points[i].ms=-500+int(i*1150/47);
  const bool referenceSaved=motion.save(t);
  const bool referenceRetained=Preferences::data==referenceBytes;
  std::cout<<"reference existing key with failed length query: opened="<<referenceOpened<<" saved="<<referenceSaved
           <<" retained="<<referenceRetained<<"\n";

  Preferences::data.clear();Preferences::zeroLengthKeys.clear();
  Preferences setting;setting.begin("launchlab-mini",false);
  Preferences::data["launchlab-minirpm_method"]={9};
  Preferences::zeroLengthKeys.insert("launchlab-minirpm_method");
  RpmEstimator::Mode mode;
  const bool settingLoaded=RpmEstimatorSetting::load(setting,mode);
  std::cout<<"method existing key with failed length query: accepted_as_missing="<<settingLoaded<<"\n";
#ifndef EXPECT_BASELINE
  assert(!historyOpened && !historySaved && historyRetained);
  assert(!referenceOpened && !referenceSaved && referenceRetained && !settingLoaded);
#else
  assert(historyOpened && historySaved && referenceOpened && referenceSaved && settingLoaded);
#endif
  std::cout<<"PASS: existing-key/failed-length fault injection\n";
}
