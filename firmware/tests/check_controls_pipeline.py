#!/usr/bin/env python3
"""Exercise actual sketch navigation/edit handlers with host peripherals."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'LaunchLabMini/LaunchLabMini.ino').read_text()
def function(name):
    start=source.index('static void '+name+'(')
    # Some handlers have a forward declaration before their definition.
    while source.index(';',start)<source.index('{',start):
        start=source.index('static void '+name+'(',start+1)
    cursor=source.index('{',start)+1
    depth=1
    while depth:
        depth+=(source[cursor]=='{')-(source[cursor]=='}')
        cursor+=1
    return source[start:cursor]
prefix=r'''
#include "controls.h"
#include "appearance_store.h"
#include "analog_tachometer.h"
#include "rpm_estimator_setting.h"
#include "display_flip_setting.h"
#include "display_orientation.h"
#include "sensor_profile_setting.h"
#include "mount_orientation_setting.h"
#include "level_indicator.h"
#include "power_status.h"
#include "practice_ui.h"
#include "launch_feedback.h"
#include "wipe_confirmation.h"
#include <cassert>
#include <iostream>
'''
# practice_ui.h needs graphics types only for its inline renderer. This fixture
# tests actions, so use the actual practice data and view types directly.
prefix=prefix.replace('#include "practice_ui.h"','#include "practice_history.h"\n#include "practice_views.h"')
practice_ui=(root/'LaunchLabMini/practice_ui.h').read_text()
prefix+=practice_ui[practice_ui.index('namespace PracticeUI {'):practice_ui.index('inline void draw(')]+'}\n'
prefix+=r'''
static Appearance::Config appearance;static Appearance::Bests personalBests;
static AppearanceStore<Appearance::Bests> bestStore;static AppearanceStore<Appearance::Config> appearanceStore;static Appearance::Celebration bestEffect;
static uint32_t millis(){return 10000;}
static unsigned soundPreviews=0,effectPreviews=0;static void previewSound(){++soundPreviews;}static void previewEffect(){++effectPreviews;}
using Controls::Page;
static Controls::State controls;
static Controls::Gesture gestureA,gestureB;
static WipeConfirmation wipeConfirmation;
static bool wipeFailed=false,wipeBlocked=false;
static struct {void cancel(){}}restoreRx;
static void beginWipe();static void cancelWipe();
static bool tournamentMode=false,historyPage=false,dirty=false,diagnostics=false,demo=false,motionDemo=false,motionPending=false,settingsSaveError=false,restorePreview=false,displayFlipped=false;
static uint8_t brightnessPercent=40;
static uint32_t historyOffset=0,newSessionAt=0,tournamentHintUntil=0;
enum class TournamentView:uint8_t{RecordingOnly,Rpm};
static TournamentView tournamentView=TournamentView::RecordingOnly;
static const char *tournamentViewName(){return tournamentView==TournamentView::Rpm?"rpm":"recording_only";}
static PracticeUI::View historyView=PracticeUI::View::Recent;
static PracticeHistory practice,demoPractice;
static struct {uint32_t number=1;} latestMotion;
static LaunchFeedback launchFeedback;
static InactivityTimer inactivity;
static RpmEstimator::Mode rpmEstimator=RpmEstimator::DEFAULT_MODE;
static StickS3SensorProfile::Mode sensorProfile=StickS3SensorProfile::DEFAULT_MODE;
static MountOrientation::Mode mountMode=MountOrientation::Mode::Top;
static StickS3Level level;static StickS3Level::Reading drawnLevel;static uint64_t lastLevelSampleUs=0;
struct MotionAcquisition {
  MountOrientation::Mode mode=MountOrientation::Mode::Top;unsigned changes=0;bool fail=false;
  bool setMount(MountOrientation::Mode next){if(fail)return false;mode=next;++changes;return true;}
}imuAcquisition;
struct Prefs:Preferences {
  unsigned writes=0;
  size_t putUChar(const char *key,uint8_t value){++writes;return putBytes(key,&value,1);}
  size_t putBytes(const char *key,const void *data,size_t size){++writes;return Preferences::putBytes(key,data,size);}
}prefs;
static uint8_t backlightValue(){return (unsigned(brightnessPercent)*255+50)/100;}
struct Snapshot{AnalogTachometer::Phase phase=AnalogTachometer::Phase::Ready;uint32_t launches=0;};
enum class Action{IdleCheckpointBegin,CheckpointEnd,Estimator,SensorProfile};
struct Acquisition {
  Snapshot state;unsigned begins=0,ends=0,estimatorChanges=0,profileChanges=0;bool failEstimator=false,failProfile=false;
  Snapshot snapshot(){return state;}
  bool request(Action action,uint32_t expected=0){
    if(action==Action::IdleCheckpointBegin){if(state.phase==AnalogTachometer::Phase::Launch || expected!=state.launches)return false;++begins;}
    if(action==Action::CheckpointEnd)++ends;
    if(action==Action::Estimator){++estimatorChanges;return !failEstimator;}
    if(action==Action::SensorProfile){++profileChanges;return !failProfile;}
    return true;
  }
}acquisition;
struct {struct {unsigned brightness=102,rotation=0;void setBrightness(unsigned value){brightness=value;}void setRotation(unsigned value){rotation=value;}uint8_t getRotation()const{return rotation;}}Display;}M5;
struct {template<class... T>void printf(const char*,T...){}void println(const char*){}}Serial;
'''
# Exercise the actual startup/wake setting load and LCD mapping, in addition
# to the full edit handlers. No copied physical-orientation implementation.
load_start=source.index('  if(!DisplayFlipSetting::load(')
load_statement=source[load_start:source.index(';',load_start)+1]
boot_rotation=re.findall(r'M5.Display.setRotation\([^;]+;',source)[-1]
prefix+='\nstatic void restoreDisplayAtBoot(){'+load_statement+boot_rotation+'}\n'
mount_load_start=source.index('  if(!MountOrientationSetting::load(')
mount_load=source[mount_load_start:source.index(';',mount_load_start)+1]
prefix+='\nstatic void restoreMountAtBoot(){'+mount_load+'assert(imuAcquisition.setMount(mountMode));}\n'
visible_start=source.index('static bool visibleDisplayFlipped()')
prefix+='\n'+source[visible_start:source.index('}',visible_start)+1]+'\n'
tests=r'''
static void reset(){
  controls={};appearance={};appearanceStore={};bestEffect={};tournamentMode=historyPage=dirty=diagnostics=demo=motionDemo=motionPending=settingsSaveError=restorePreview=displayFlipped=false;
  gestureA={};gestureB={};wipeConfirmation={};wipeFailed=wipeBlocked=false;
  historyOffset=newSessionAt=tournamentHintUntil=0;historyView=PracticeUI::View::Recent;tournamentView=TournamentView::RecordingOnly;
  practice={};demoPractice={};latestMotion={};launchFeedback={};inactivity={};brightnessPercent=40;rpmEstimator=RpmEstimator::DEFAULT_MODE;sensorProfile=StickS3SensorProfile::DEFAULT_MODE;
  acquisition={};prefs={};M5.Display.brightness=102;M5.Display.rotation=2;
  mountMode=MountOrientation::Mode::Top;imuAcquisition={};level={};drawnLevel={};lastLevelSampleUs=0;
  Preferences::data.clear();Preferences::zeroLengthKeys.clear();Preferences::failWrite=Preferences::shortRead=false;
  assert(prefs.begin("controls-test",false));assert(appearanceStore.begin(appearance,"controls-appearance","cfg0","cfg1"));
}
static void edit(unsigned row){controls.page=Page::Settings;controls.setting=row;syncNavigation();buttonA();assert(controls.page==Page::Edit);}
int main(){
  reset();restoreMountAtBoot();assert(mountMode==MountOrientation::Mode::Top && !prefs.writes);
  for(auto selected:{MountOrientation::Mode::Sidecar,MountOrientation::Mode::Top}) {
    assert(MountOrientationSetting::save(prefs,selected));const auto stored=Preferences::data;
    mountMode=MountOrientation::Mode(99);restoreMountAtBoot();
    assert(mountMode==selected && imuAcquisition.mode==selected && Preferences::data==stored);
  }
  reset();restoreDisplayAtBoot();assert(!displayFlipped && M5.Display.rotation==2 && !prefs.writes);
  for(bool saved:{true,false}) {
    assert(DisplayFlipSetting::save(prefs,saved));const auto stored=Preferences::data;
    displayFlipped=!saved;M5.Display.rotation=99;restoreDisplayAtBoot();
    assert(displayFlipped==saved && M5.Display.rotation==(saved?0u:2u) && Preferences::data==stored);
  }
  reset();buttonA();holdB();assert(!launchFeedback.shown() && controls.page==Page::Main);
  reset();edit(2);acquisition.state.phase=AnalogTachometer::Phase::Launch;buttonB();
  assert(controls.draft==50 && brightnessPercent==40 && M5.Display.brightness==102 && restorePreview);
  acquisition.state.phase=AnalogTachometer::Phase::Ready;previewSetting();
  assert(M5.Display.brightness==128 && !restorePreview);holdB();assert(M5.Display.brightness==102 && !prefs.writes);
  reset();buttonA();assert(launchFeedback.shown());buttonB();assert(!launchFeedback.shown() && controls.page==Page::Menu);
  buttonA();assert(controls.page==Page::HistoryMenu);buttonA();assert(controls.page==Page::History);
  assert(practice.accept(1,true,6000,millis()));buttonB();holdB();assert(controls.page==Page::HistoryMenu);
  holdB();holdB();assert(controls.page==Page::Main && !historyPage);
  reset();edit(2);buttonB();assert(controls.draft==50 && brightnessPercent==40 && M5.Display.brightness==128 && !prefs.writes);
  holdB();assert(controls.page==Page::Settings && brightnessPercent==40 && M5.Display.brightness==102 && !prefs.writes);
  edit(2);buttonB();buttonA();assert(controls.page==Page::Settings && brightnessPercent==50 && M5.Display.brightness==128 && prefs.writes && !settingsSaveError);
  assert(acquisition.begins==acquisition.ends);
  reset();edit(3);assert(!visibleDisplayFlipped());buttonB();assert(!displayFlipped && visibleDisplayFlipped() && M5.Display.rotation==0 && !prefs.writes);
  holdB();assert(!visibleDisplayFlipped() && M5.Display.rotation==2 && !prefs.writes);
  edit(3);buttonB();Preferences::failWrite=true;buttonA();assert(controls.page==Page::Edit && settingsSaveError && !displayFlipped);
  Preferences::failWrite=false;buttonA();assert(controls.page==Page::Settings && displayFlipped && visibleDisplayFlipped() && M5.Display.rotation==0);
  const auto flipStored=Preferences::data;edit(3);buttonB();assert(!visibleDisplayFlipped() && displayFlipped && M5.Display.rotation==2);
  holdB();assert(visibleDisplayFlipped() && M5.Display.rotation==0 && Preferences::data==flipStored);
  // During a pull the display changes are deferred; cancel returns to the
  // saved orientation as soon as idle without persisting the draft.
  edit(3);acquisition.state.phase=AnalogTachometer::Phase::Launch;buttonB();
  assert(visibleDisplayFlipped() && M5.Display.rotation==0 && restorePreview);
  holdB();assert(visibleDisplayFlipped() && M5.Display.rotation==0 && restorePreview && Preferences::data==flipStored);
  acquisition.state.phase=AnalogTachometer::Phase::Ready;previewSetting();assert(!restorePreview && M5.Display.rotation==0);
  reset();edit(0);for(unsigned i=0;i<7;++i)buttonB();assert(controls.draft==10 && inactivity.minutes()==3 && !prefs.writes);
  acquisition.state.phase=AnalogTachometer::Phase::Launch;buttonA();assert(controls.page==Page::Edit && !prefs.writes && settingsSaveError);
  acquisition.state.phase=AnalogTachometer::Phase::Ready;buttonA();assert(inactivity.minutes()==10 && controls.page==Page::Settings);
  reset();assert(practice.accept(1,true,6000,millis()));const auto session=practice.session()->number;
  edit(1);buttonB();assert(rpmEstimator==RpmEstimator::Mode::ThreeTurn && practice.session()->number==session);
  holdB();assert(practice.accept(2,true,6100,millis()) && practice.session()->number==session);
  edit(1);buttonB();buttonA();assert(rpmEstimator==RpmEstimator::Mode::SingleTurn && acquisition.estimatorChanges==1);
  assert(practice.accept(3,true,6200,millis()) && practice.session()->number==session+1 && practice.size()==3);
  reset();edit(1);buttonB();acquisition.failEstimator=true;buttonA();assert(controls.page==Page::Edit && rpmEstimator==RpmEstimator::Mode::ThreeTurn && settingsSaveError);
  RpmEstimator::Mode saved;assert(RpmEstimatorSetting::load(prefs,saved) && saved==rpmEstimator);
  reset();assert(practice.accept(1,true,6000,millis()));const auto profileSession=practice.session()->number;
  edit(4);buttonB();assert(controls.draft==1 && sensorProfile==StickS3SensorProfile::Mode::Standard && !prefs.writes);
  holdB();assert(sensorProfile==StickS3SensorProfile::Mode::Standard && !prefs.writes && !acquisition.profileChanges);
  edit(4);buttonB();acquisition.state.phase=AnalogTachometer::Phase::Launch;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && !prefs.writes && !acquisition.profileChanges);
  acquisition.state.phase=AnalogTachometer::Phase::Ready;buttonA();
  assert(controls.page==Page::Settings && sensorProfile==StickS3SensorProfile::Mode::Tcrt && acquisition.profileChanges==1);
  StickS3SensorProfile::Mode profileSaved;
  assert(SensorProfileSetting::load(prefs,profileSaved) && profileSaved==sensorProfile);
  assert(practice.accept(2,true,6100,millis()) && practice.session()->number==profileSession+1);
  const auto writes=prefs.writes;edit(4);buttonA();assert(acquisition.profileChanges==1 && prefs.writes==writes);
  reset();edit(4);buttonB();acquisition.failProfile=true;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && sensorProfile==StickS3SensorProfile::Mode::Standard);
  assert(SensorProfileSetting::load(prefs,profileSaved) && profileSaved==sensorProfile);
  reset();edit(4);buttonB();Preferences::failWrite=true;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && !acquisition.profileChanges && sensorProfile==StickS3SensorProfile::Mode::Standard);
  Preferences::failWrite=false;buttonA();assert(sensorProfile==StickS3SensorProfile::Mode::Tcrt && !settingsSaveError);
  assert(acquisition.begins==acquisition.ends);
  reset();const auto untouched=practice.generation();edit(5);buttonB();assert(controls.draft==1 && appearance.theme==Appearance::Theme::Classic);holdB();assert(appearance.theme==Appearance::Theme::Classic);
  edit(5);buttonB();buttonA();assert(appearance.theme==Appearance::Theme::Mint && practice.generation()==untouched && !acquisition.estimatorChanges && !acquisition.profileChanges);
  // Random is the default and comes first; B steps to Orbit and previews it.
  edit(6);assert(controls.draft==uint8_t(Appearance::Effect::Random));const unsigned shown=effectPreviews;buttonB();assert(effectPreviews==shown+1);
  Preferences::failWrite=true;buttonA();assert(settingsSaveError && appearance.effect==Appearance::Effect::Random && controls.page==Page::Edit);
  Preferences::failWrite=false;buttonA();assert(appearance.effect==Appearance::Effect::Orbit && controls.page==Page::Settings);
  Appearance::Config restored;AppearanceStore<Appearance::Config> reader;assert(reader.begin(restored,"controls-appearance","cfg0","cfg1") && restored.theme==appearance.theme && restored.effect==appearance.effect);
  // Sound: each B previews the draft level; only Apply saves it, with the theme and effect intact.
  edit(7);assert(controls.draft==0);const unsigned previews=soundPreviews;buttonB();buttonB();
  assert(controls.draft==2 && soundPreviews==previews+2 && appearance.sound==Appearance::Sound::Off);
  holdB();assert(appearance.sound==Appearance::Sound::Off);
  edit(7);buttonB();buttonA();assert(appearance.sound==Appearance::Sound::Low && controls.page==Page::Settings);
  {Appearance::Config again;AppearanceStore<Appearance::Config> reread;assert(reread.begin(again,"controls-appearance","cfg0","cfg1") &&
    again.sound==Appearance::Sound::Low && again.theme==Appearance::Theme::Mint && again.effect==Appearance::Effect::Orbit);}
  // Mount drafts never change live axes or flash. Apply waits until optical
  // idle, resets live level, and preserves scalar history/session and Flip.
  reset();assert(practice.accept(1,true,6000,millis()));const auto mountSession=practice.session()->number;
  level.feed(.1f,0,1,millis());drawnLevel=level.reading(millis());lastLevelSampleUs=12345;
  edit(8);assert(controls.draft==0);buttonB();
  assert(controls.draft==1 && mountMode==MountOrientation::Mode::Top && !prefs.writes && !imuAcquisition.changes);
  holdB();assert(mountMode==MountOrientation::Mode::Top && !prefs.writes && !imuAcquisition.changes);
  edit(8);buttonB();acquisition.state.phase=AnalogTachometer::Phase::Launch;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && !prefs.writes && !imuAcquisition.changes);
  acquisition.state.phase=AnalogTachometer::Phase::Ready;motionPending=true;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && !prefs.writes && !imuAcquisition.changes);
  motionPending=false;buttonA();
  assert(mountMode==MountOrientation::Mode::Sidecar && imuAcquisition.mode==mountMode && imuAcquisition.changes==1);
  assert(controls.page==Page::Settings && !settingsSaveError && !displayFlipped && M5.Display.rotation==2);
  assert(!level.reading(millis()).valid() && !drawnLevel.valid() && lastLevelSampleUs==0);
  assert(practice.accept(2,true,6100,millis()) && practice.session()->number==mountSession && practice.size()==2);
  MountOrientation::Mode mountSaved;assert(MountOrientationSetting::load(prefs,mountSaved) && mountSaved==mountMode);
  const auto mountBytes=Preferences::data;const auto mountChanges=imuAcquisition.changes;
  edit(8);buttonA();assert(Preferences::data==mountBytes && imuAcquisition.changes==mountChanges);
  edit(8);buttonB();imuAcquisition.fail=true;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && mountMode==MountOrientation::Mode::Sidecar);
  assert(MountOrientationSetting::load(prefs,mountSaved) && mountSaved==mountMode);
  imuAcquisition.fail=false;Preferences::failWrite=true;buttonA();
  assert(controls.page==Page::Edit && settingsSaveError && imuAcquisition.changes==mountChanges);
  Preferences::failWrite=false;buttonA();assert(mountMode==MountOrientation::Mode::Top && !settingsSaveError);
  assert(acquisition.begins==acquisition.ends);
  reset();holdA();assert(tournamentMode && tournamentView==TournamentView::RecordingOnly);
  buttonA();holdB();assert(tournamentMode && !launchFeedback.shown());buttonB();assert(tournamentView==TournamentView::Rpm);
  holdA();assert(!tournamentMode && controls.page==Page::Main);
  reset();edit(3);buttonB();cancelSettingEdit();syncNavigation();assert(controls.page==Page::Settings && M5.Display.rotation==2 && !prefs.writes);
  reset();controls.page=Page::Settings;controls.setting=unsigned(Controls::Setting::WipeAll);buttonA();
  assert(controls.page==Page::Wipe && historyPage && wipeConfirmation.active() && !prefs.writes);
  for(unsigned i=0;i<50;++i){buttonA();holdA();}
  assert(wipeConfirmation.stage()==WipeConfirmation::Stage::Warning && !prefs.writes);
  holdB();assert(controls.page==Page::Settings && !wipeConfirmation.active() && !prefs.writes);
  buttonA();wipeConfirmation.tick(millis(),false,false);buttonA();
  assert(wipeConfirmation.stage()==WipeConfirmation::Stage::Confirm);
  buttonB();assert(controls.page==Page::Settings && !wipeConfirmation.active() && !prefs.writes);
  buttonA();wipeBlocked=true;holdB();assert(controls.page==Page::Wipe && !wipeConfirmation.active());
  std::cout<<"PASS: actual control handlers; menu/back, recap, preview rollback, persistence retry, active-pull guard, metric/session boundary, rollback, sound preview/apply and tournament lock\n";
}
'''
names=['syncNavigation','toggleFeedback','previewSetting','beginWipe','cancelWipe','cancelSettingEdit','commitSetting','newSession','browseOlder','navigate','buttonA','buttonB','holdA','holdB']
with tempfile.TemporaryDirectory(prefix='launchlab-controls-') as directory:
    directory=Path(directory);cpp=directory/'test.cpp';exe=directory/'test'
    cpp.write_text(prefix+'\n'.join(function(name) for name in names)+tests)
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I'+str(root/'tests/fakes'),'-I'+str(root/'LaunchLabMini'),'-I'+str(root/'LaunchLabRpm'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
