#include "../LaunchLabMini/sensor_profile_setting.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace StickS3SensorProfile;
static void level(AnalogTachometer &d,unsigned us,unsigned value) {
  for(unsigned i=0;i<us/20;++i)d.feed(value);
}
int main() {
  Preferences prefs;prefs.begin("launchlab-mini",false);
  const uint8_t other[]={4,5,6};prefs.putBytes("history0",other,sizeof(other));
  prefs.putBytes("rpm_method",other,sizeof(other));
  const auto original=Preferences::data;
  Mode mode=Mode::Tcrt;
  assert(SensorProfileSetting::load(prefs,mode) && mode==Mode::Standard && Preferences::data==original);
  for(Mode choice:{Mode::Tcrt,Mode::Standard}) {
    assert(SensorProfileSetting::save(prefs,choice));
    Preferences reboot;reboot.begin("launchlab-mini",false);
    assert(SensorProfileSetting::load(reboot,mode) && mode==choice);
    for(const auto &entry:original)assert(Preferences::data.at(entry.first)==entry.second);
  }
  const auto before=Preferences::data;Preferences::failWrite=true;
  assert(!SensorProfileSetting::save(prefs,Mode::Tcrt) && Preferences::data==before);
  Preferences::failWrite=false;
  assert(!SensorProfileSetting::save(prefs,Mode(2)) && Preferences::data==before);
  for(unsigned index:{0u,1u,2u,3u}) {
    assert(SensorProfileSetting::save(prefs,Mode::Tcrt));
    auto &record=Preferences::data["launchlab-minisensor_profile"];record[index]^=4;
    const auto corrupt=Preferences::data;
    assert(!SensorProfileSetting::load(prefs,mode) && mode==Mode::Standard && Preferences::data==corrupt);
  }
  assert(SensorProfileSetting::save(prefs,Mode::Tcrt));
  Preferences::shortRead=true;assert(!SensorProfileSetting::load(prefs,mode) && mode==Mode::Standard);
  Preferences::shortRead=false;
  Preferences::zeroLengthKeys.insert("launchlab-minisensor_profile");
  assert(!SensorProfileSetting::load(prefs,mode) && mode==Mode::Standard);
  Preferences::zeroLengthKeys.clear();
  // Same executable, different saved runtime choice: weak real-sized marks
  // are usable for TCRT, while the standard floor remains protective.
  for(Mode choice:{Mode::Standard,Mode::Tcrt})for(unsigned amplitude:{45u,59u,63u,71u,100u}) {
    AnalogTachometer d;configure(d,choice);level(d,1200000,140);
    for(unsigned turn=0;turn<12;++turn){level(d,6000,140+amplitude);level(d,6000,140);}
    level(d,1600000,140);
    if(amplitude>=minMark(choice))assert(d.launches==1 && std::fabs(d.resultRpm-5000)<3);
    else assert(d.launches==0);
  }
  std::cout<<"PASS: both runtime sensor profiles; independent timing oracle, weak-mark rejection, saved choices survive reload, unrelated keys retained, corrupt/failed-read/write handling\n";
}
