#pragma once
#include "controls.h"
#include "rpm_estimator.h"
#include "sensor_profile.h"
#include "appearance.h"

namespace ControlsUI {
static constexpr const char *MENU[]={"History","New session","Battery","Settings","Tournament","Back"};
static constexpr const char *HISTORY[]={"Launches","Pull trend","Session trend","Back"};
static constexpr const char *SETTINGS[]={"Sleep after","RPM method","Brightness","Flip display","Sensor profile","Theme","Best effect","Sound","Back"};
// Updated by the UI owner before rendering; warnings/status keep their colors.
static uint16_t accent=TFT_CYAN;
inline void footer(lgfx::LGFXBase &g,const char *label) {
  g.fillRect(0,229,g.width(),11,TFT_BLACK);
  g.setTextDatum(top_center);g.setTextColor(TFT_DARKGREY,TFT_BLACK);
  g.drawString(label,g.width()/2,231,1);
}
inline void heading(lgfx::LGFXBase &g,const char *label) {
  g.setTextDatum(top_center);g.setTextColor(TFT_WHITE,TFT_BLACK);
  g.drawString(label,g.width()/2,8,2);
}
inline void list(lgfx::LGFXBase &g,const char *title,const char *const *labels,unsigned count,unsigned selected) {
  heading(g,title);
  for(unsigned i=0;i<count;++i) {
    const int y=42+int(i)*27;
    if(i==selected)g.drawRoundRect(4,y-4,g.width()-8,25,4,accent);
    g.setTextColor(i==selected?accent:TFT_WHITE,TFT_BLACK);
    g.drawString(labels[i],g.width()/2,y,2);
  }
  footer(g,selected+1==count?"A Back  B Next":"A Open  B Next");
}
inline void value(char *out,unsigned length,unsigned setting,unsigned value) {
  switch(Controls::Setting(setting)) {
    case Controls::Setting::Sleep:snprintf(out,length,"%u min",value);break;
    case Controls::Setting::Rpm:snprintf(out,length,"%s",RpmEstimator::label(RpmEstimator::Mode(value)));break;
    case Controls::Setting::Brightness:snprintf(out,length,"%u%%",value);break;
    case Controls::Setting::Flip:snprintf(out,length,"%s",value?"On":"Off");break;
    case Controls::Setting::Sensor:snprintf(out,length,"%s",StickS3SensorProfile::label(StickS3SensorProfile::Mode(value)));break;
    case Controls::Setting::Theme:snprintf(out,length,"%s",Appearance::label(Appearance::Theme(value)));break;
    case Controls::Setting::BestEffect:snprintf(out,length,"%s",Appearance::label(Appearance::Effect(value)));break;
    case Controls::Setting::Sound:snprintf(out,length,"%s",Appearance::label(Appearance::Sound(value)));break;
  }
}
inline void settings(lgfx::LGFXBase &g,const Controls::State &state,const uint8_t *values) {
  heading(g,"Settings");char text[24];
  const unsigned first=state.setting<5?0:5,last=first?Controls::SETTING_COUNT:4;
  for(unsigned i=first;i<=last;++i) {
    const int y=33+int(i-first)*31;
    if(i==state.setting)g.drawRoundRect(4,y-3,g.width()-8,30,4,accent);
    g.setTextColor(i==state.setting?accent:TFT_WHITE,TFT_BLACK);
    g.drawString(SETTINGS[i],g.width()/2,y,2);
    if(i<Controls::SETTING_COUNT){value(text,sizeof(text),i,values[i]);g.drawString(text,g.width()/2,y+18,1);}
  }
  if(!first){g.setTextColor(TFT_DARKGREY,TFT_BLACK);g.drawString("B Next: appearance",g.width()/2,194,1);}
  footer(g,state.setting<Controls::SETTING_COUNT?"A Edit  B Next":"A Back  B Next");
}
inline void editor(lgfx::LGFXBase &g,const Controls::State &state,bool failed,bool changedMetric) {
  heading(g,SETTINGS[state.setting]);char text[24];value(text,sizeof(text),state.setting,state.draft);
  g.setTextColor(TFT_DARKGREY,TFT_BLACK);g.drawString("Hold B cancels",g.width()/2,35,1);
  // Center the value at full width, with a quiet cycle hint below its card.
  g.drawRoundRect(4,68,g.width()-8,64,5,TFT_DARKGREY);
  const int valueWidth=g.width()-16,valueX=g.width()/2;
  // Palette names share one readable size while cycling between all ten choices.
  const int valueFont=state.setting==unsigned(Controls::Setting::Theme)?2:
    g.textWidth(text,&fonts::Font4)<=valueWidth?4:
    g.textWidth(text,&fonts::Font2)<=valueWidth?2:1;
  g.setTextColor(accent,TFT_BLACK);
  g.drawString(text,valueX,valueFont==4?86:valueFont==2?91:95,valueFont);
  g.setTextColor(0x39e7,TFT_BLACK);g.drawString("B next option ->",g.width()/2,139,1);
  if(changedMetric) {
    g.setTextColor(TFT_WHITE,TFT_BLACK);
    g.drawString("Next pull starts",g.width()/2,154,1);
    g.drawString("a new session",g.width()/2,169,1);
  }
  if(failed){g.setTextColor(TFT_ORANGE,TFT_BLACK);g.drawString("Try again when idle",g.width()/2,196,1);}
  footer(g,"A Apply  B Next");
}
inline void session(lgfx::LGFXBase &g,bool start) {
  heading(g,"New session");g.setTextColor(TFT_WHITE,TFT_BLACK);
  g.drawString("Keep past launches",g.width()/2,42,1);
  const char *labels[]={"Cancel","Start"};
  for(unsigned i=0;i<2;++i) {
    const int y=91+int(i)*42;
    if(bool(i)==start)g.drawRoundRect(12,y-5,g.width()-24,32,4,accent);
    g.setTextColor(bool(i)==start?accent:TFT_WHITE,TFT_BLACK);
    g.drawString(labels[i],g.width()/2,y,2);
  }
  footer(g,"A Choose  B Next");
}
inline void hold(lgfx::LGFXBase &g,const char *label,unsigned elapsed,unsigned duration) {
  footer(g,label);
  g.fillRect(4,225,int((g.width()-8)*std::min(elapsed,duration)/duration),2,accent);
}
}
