#include "../LaunchLabMini/display_flip_setting.h"
#include "../LaunchLabMini/display_orientation.h"
#include <cassert>
#include <iostream>

int main() {
  Preferences prefs;prefs.begin("launchlab-mini",false);
  const uint8_t prior[]={4,8,15,16,23,42};
  for(const char *key:{"sleep_min","bright_pct","rpm_method","wake_ui","history0","reference0"})
    prefs.putBytes(key,prior,sizeof(prior));
  const auto existing=Preferences::data;
  bool flipped=true;
  assert(DisplayFlipSetting::load(prefs,flipped) && !flipped);
  static_assert(DisplayOrientation::rotation()==2 && DisplayOrientation::axisSign()==-1,
    "Flip Off remains the default and now uses rotation 2");
  static_assert(DisplayOrientation::rotation(true)==0 && DisplayOrientation::axisSign(true)==1,
    "Flip On uses the former Off orientation");
  assert(Preferences::data==existing);
  // Existing schema-1 Off/On records retain their boolean and exact bytes.
  for(uint8_t selected:{0,1}) {
    const uint8_t legacy[]={0x44,1,selected,uint8_t(0x44^1^selected)};
    prefs.putBytes(DisplayFlipSetting::KEY,legacy,sizeof(legacy));const auto before=Preferences::data;
    assert(DisplayFlipSetting::load(prefs,flipped) && flipped==bool(selected));
    assert(DisplayOrientation::rotation(flipped)==(selected?0:2) && Preferences::data==before);
  }
  for(bool selected:{true,false,true}) {
    assert(DisplayFlipSetting::save(prefs,selected));
    Preferences reboot;reboot.begin("launchlab-mini",false);
    assert(DisplayFlipSetting::load(reboot,flipped) && flipped==selected);
    for(const auto &entry:existing)assert(Preferences::data.at(entry.first)==entry.second);
  }
  const auto good=Preferences::data;
  Preferences::failWrite=true;
  assert(!DisplayFlipSetting::save(prefs,false) && Preferences::data==good);
  Preferences::failWrite=false;Preferences::shortRead=true;
  assert(!DisplayFlipSetting::load(prefs,flipped) && !flipped);
  assert(Preferences::data==good);
  Preferences::shortRead=false;
  auto &record=Preferences::data[std::string("launchlab-mini")+DisplayFlipSetting::KEY];
  record[2]=2;
  const auto corrupt=Preferences::data;
  assert(!DisplayFlipSetting::load(prefs,flipped) && !flipped && Preferences::data==corrupt);
  Preferences::zeroLengthKeys.insert(std::string("launchlab-mini")+DisplayFlipSetting::KEY);
  assert(!DisplayFlipSetting::load(prefs,flipped) && !flipped && Preferences::data==corrupt);
  std::cout<<"PASS: Flip Off default maps to rotation 2 without writes, existing Off/On bytes preserved, flip survives restart, other settings/history retained, failed writes and invalid reads retained safely\n";
}
