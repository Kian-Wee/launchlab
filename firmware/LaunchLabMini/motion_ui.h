#pragma once
#include "launch_motion.h"
#include "level_indicator.h"
#include "motion_replay.h"
#include "palette.h"

namespace MotionUI {
inline const char *unavailableTilt(LaunchMotion::Quality quality) {
  switch(quality) {
    case LaunchMotion::Quality::Warming:return "Tilt warming up";
    case LaunchMotion::Quality::Gap:return "Tilt incomplete";
    case LaunchMotion::Quality::Clipped:return "Tilt out of range";
    case LaunchMotion::Quality::TooLong:return "Pull too long";
    default:return "No tilt data";
  }
}
inline void spatial(lgfx::LGFXBase &g,const LaunchMotion::Trace &trial,
                    int top,int height,const Palette::Colors &c,float progress=1,bool flipEnabled=false) {
  using namespace MotionReplay;
  // Fixed camera/scale and fused orientation only. Cached geometry lives off
  // the UI task stack; no live IMU, acquisition, travel or height inference.
  static Replay current;static uint32_t lastCurrent=0;
  auto signature=[](const LaunchMotion::Trace &t){
    return LaunchMotion::checksum(reinterpret_cast<const uint8_t *>(t.points),sizeof(t.points))^
      LaunchMotion::checksum(reinterpret_cast<const uint8_t *>(t.gravity),sizeof(t.gravity))^
      t.durationMs^(t.count<<16)^(unsigned(t.quality)<<24)^(unsigned(t.fused)<<30);
  };
  const auto a=signature(trial);
  if(a!=lastCurrent){current.build(trial);lastCurrent=a;}
  if(!current.valid)return;
  Cube cube;Vec lo{1e9f,1e9f,0},hi{-1e9f,-1e9f,0};
  for(unsigned i=0;i<8;++i){const Vec p=camera(cube.corner(i));lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);}
  const float scale=std::min((g.width()-12)/(hi.x-lo.x),(height-4)/(hi.y-lo.y));
  auto project=[&](Vec v){const Vec p=camera(v);return Vec{g.width()*.5f+(p.x-(lo.x+hi.x)*.5f)*scale,top+height*.5f+(p.y-(lo.y+hi.y)*.5f)*scale,0};};
  auto line=[&](Vec a,Vec b,uint16_t color){a=project(a);b=project(b);g.drawLine(lroundf(a.x),lroundf(a.y),lroundf(b.x),lroundf(b.y),color);};
  static constexpr unsigned edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
  // Quiet rear edges, brighter near edges, a stationary mid-height plane and
  // a tiny up arrow give a gravity reference without obscuring the body.
  for(const auto &e:edges)line(cube.corner(e[0]),cube.corner(e[1]),c.sceneRear);
  for(const unsigned i:{0u,1u,5u,4u})line(cube.corner(i),cube.corner((i+1)%4+(i>=4?4:0)),c.faint);
  line({-.84f,-.84f,0},{.84f,-.84f,0},c.scenePlane);
  line({.84f,-.84f,0},{.84f,.84f,0},c.scenePlane);
  g.drawLine(9,top+20,9,top+5,c.muted);g.drawLine(9,top+5,6,top+9,c.muted);g.drawLine(9,top+5,12,top+9,c.muted);
  progress=std::max(0.0f,std::min(1.0f,progress));
  // Full recorded context at original speed. Start/End circles retain optical
  // onset/end tilt, independent of the pre/post context shown in the replay.
  const auto pose=forDisplay(current.at(trial.points[0].ms+lroundf(progress*playbackDurationMs(trial))),flipEnabled);
  auto body=[&](const LaunchMotion::Quaternion &q,unsigned vertex){
    static constexpr unsigned bits[8]={0,1,3,2,4,5,7,6};const unsigned v=bits[vertex];
    return rotate(q,{(v&1?1:-1)*.4f,(v&2?1:-1)*.65f,(v&4?1:-1)*.12f});
  };
  Vec corners[8];for(unsigned i=0;i<8;++i)corners[i]=project(body(pose,i));
  auto face=[&](unsigned a,unsigned b,unsigned c,unsigned d,uint16_t color){
    g.fillTriangle(lroundf(corners[a].x),lroundf(corners[a].y),lroundf(corners[b].x),lroundf(corners[b].y),lroundf(corners[c].x),lroundf(corners[c].y),color);
    g.fillTriangle(lroundf(corners[a].x),lroundf(corners[a].y),lroundf(corners[c].x),lroundf(corners[c].y),lroundf(corners[d].x),lroundf(corners[d].y),color);
  };
  static constexpr unsigned faces[6][4]={{0,3,7,4},{1,2,6,5},{0,1,5,4},{3,2,6,7},{0,1,2,3},{4,5,6,7}};
  static constexpr Vec normal[6]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
  // Blue screen, violet back and dark sides provide front/back identity: the
  // StickS3's own colors, the same in every theme.
  static constexpr uint16_t colors[6]={0x1947,0x2169,0x298a,0x298a,0x50ea,0x0396};
  for(unsigned f=0;f<6;++f){const auto n=rotate(pose,normal[f]);if(n.x-n.y+n.z>0)face(faces[f][0],faces[f][1],faces[f][2],faces[f][3],colors[f]);}
  for(const auto &e:edges)line(body(pose,e[0]),body(pose,e[1]),0x5dff);
  // Hardware features use physical board coordinates. At rotation 2 (Flip Off),
  // USB is at logical +Y rather than the displayed bottom (-Y).
  const float hardwareSign=DisplayOrientation::axisSign(flipEnabled);
  auto hardware=[&](Vec v){return rotate(pose,{v.x*hardwareSign,v.y*hardwareSign,v.z});};
  // Thick white USB cap remains identifiable even when the screen faces away.
  const Vec usbA=project(hardware({-.4f,-.65f,0})),usbB=project(hardware({.4f,-.65f,0}));
  for(int offset=-1;offset<=1;++offset)g.drawLine(lroundf(usbA.x),lroundf(usbA.y)+offset,lroundf(usbB.x),lroundf(usbB.y)+offset,Palette::WHITE);
  // An inset rectangle identifies the screen; omit it when looking at the back.
  const auto front=rotate(pose,{0,0,1});
  if(front.x-front.y+front.z>0) {
    const Vec screen[4]={{-.28f,-.38f,.125f},{.28f,-.38f,.125f},{.28f,.43f,.125f},{-.28f,.43f,.125f}};
    for(unsigned i=0;i<4;++i)line(hardware(screen[i]),hardware(screen[(i+1)%4]),0x7e9f);
  }
}

// A separate page for the last launch's initial direction, inferred only from
// the saved fused end-tilt. This deliberately does not estimate displacement.
inline void direction(lgfx::LGFXBase &g,const LaunchMotion::Trace &trial,
                      bool capturing,bool flipEnabled,
                      const Palette::Colors &c=Palette::builtIn(0x07ff,true)) {
  const int cx=g.width()/2,cy=122,radius=42;
  g.setTextDatum(top_center);
  g.setTextColor(c.text,c.background);g.drawString("Launch direction",cx,8,2);
  if(trial.number) {
    char number[24];snprintf(number,sizeof(number),"Launch #%lu",(unsigned long)trial.number);
    g.setTextColor(c.muted,c.background);g.drawString(number,cx,29,1);
  }
  if(capturing) {
    g.setTextColor(c.warning,c.background);g.drawString("Capturing tilt...",cx,105,2);
    return;
  }
  if(!trial.valid() || !trial.fused || !trial.endLevel.valid()) {
    g.setTextColor(c.warning,c.background);
    g.drawString(trial.quality==LaunchMotion::Quality::Missing?"No launch tilt":unavailableTilt(trial.quality),cx,106,1);
    return;
  }
  const auto tilt=StickS3Level::forDisplay(trial.endLevel,flipEnabled);
  g.setTextColor(c.muted,c.background);
  g.drawCircle(cx,cy,radius,c.muted);
  g.drawFastHLine(cx-radius,cy,2*radius+1,c.faint);
  g.drawFastVLine(cx,cy-radius,2*radius+1,c.faint);
  g.drawString("FORWARD",cx,59,1);
  g.drawString("BACKWARD",cx,170,1);
  g.setTextDatum(middle_left);g.drawString("L",8,cy-4,1);
  g.setTextDatum(middle_right);g.drawString("R",g.width()-8,cy-4,1);
  g.setTextDatum(top_center);
  if(tilt.degrees<3.0f || std::hypot(tilt.right,tilt.down)<0.08f) {
    g.fillCircle(cx,cy,4,c.accent);
    g.setTextColor(c.accent,c.background);g.drawString("CENTRE",cx,190,2);
  } else {
    // The level indicator's screen-down component corresponds to backward
    // tilt in this relative diagram. Forward is therefore -down.
    float x=tilt.right,forward=-tilt.down;
    const float magnitude=std::hypot(x,forward);
    x/=magnitude;forward/=magnitude;
    const int dx=lroundf(x*31),dy=lroundf(-forward*31);
    const int ex=cx+dx,ey=cy+dy;
    g.drawCircle(cx,cy,3,c.muted);g.drawLine(cx,cy,ex,ey,c.accent);
    const float length=std::sqrt(float(dx*dx+dy*dy));
    if(length>1) {
      const int hx=lroundf(ex-7*dx/length),hy=lroundf(ey-7*dy/length);
      const float px=-float(dy)/length,py=float(dx)/length;
      g.fillTriangle(ex,ey,lroundf(hx+3*px),lroundf(hy+3*py),lroundf(hx-3*px),lroundf(hy-3*py),c.accent);
    }
    float angle=std::atan2(x,forward)*57.2957795f;if(angle<0)angle+=360;
    const char *label=angle<22.5f || angle>=337.5f?"FORWARD":
      angle<67.5f?"FRONT-RIGHT":angle<112.5f?"RIGHT":angle<157.5f?"BACK-RIGHT":
      angle<202.5f?"BACKWARD":angle<247.5f?"BACK-LEFT":angle<292.5f?"LEFT":"FRONT-LEFT";
    g.setTextColor(c.accent,c.background);g.drawString(label,cx,190,2);
  }
  g.setTextColor(c.muted,c.background);g.drawString("Estimated from launch tilt",cx,212,1);
}
inline void feedback(lgfx::LGFXBase &g,const LaunchMotion::Trace &trial,
                     bool capturing,bool demo=false,float progress=1,bool flipEnabled=false,
                     const Palette::Colors &c=Palette::builtIn(0x07ff,true),const char *badge=nullptr) {
  const int cx=g.width()/2;char text[64];
  g.setTextDatum(top_center);
  // A celebrated pull's label: NEW BEST, or the milestone it reached.
  g.setTextColor(demo?c.warning:badge?c.accent:c.muted,c.background);g.drawString(demo?"DEMO":badge?badge:"Last launch",cx,2,1);
  snprintf(text,sizeof(text),"%.0f RPM",trial.rpm);g.setTextColor(c.accent,c.background);
  g.drawString(text,cx,17,g.textWidth(text,&fonts::Font4)<=g.width()-6?4:2);
  if(trial.valid() && trial.fused && !capturing) {
    spatial(g,trial,49,112,c,progress,flipEnabled);
    // A full-scale reading or a bridged short gap is approximate: still show it, marked.
    g.setTextColor(trial.approximate()?c.warning:c.muted,c.background);
    g.drawString(trial.approximate()?"Tilt approx.":"Tilt replay",cx,164,1);
    g.drawFastHLine(26,178,83,c.faint);
    g.fillCircle(26+lroundf(std::max(0.0f,std::min(1.0f,progress))*82),178,2,c.accent);
  } else {
    // Keep the graph area quiet when capture is pending or unavailable.
    g.drawRect(cx-39,71,79,79,c.muted);
    g.setTextColor(c.muted,c.background);g.drawString(capturing?"Capturing":unavailableTilt(trial.quality),cx,104,1);
  }
  for(unsigned i=0;i<2;++i) {
    const int bx=i?99:36,by=211,radius=17;
    const auto level=StickS3Level::forDisplay(i?trial.endLevel:trial.startLevel,flipEnabled);
    g.setTextColor(c.muted,c.background);g.drawString(i?"End":"Start",bx,183,1);
    g.drawCircle(bx,by,radius,c.muted);g.drawFastHLine(bx-radius+2,by,2*radius-3,c.muted);
    g.drawFastVLine(bx,by-radius+2,2*radius-3,c.muted);
    if(level.valid() && trial.fused && !capturing) {
      const auto color=level.degrees<=2?c.accent:c.warning;
      g.fillCircle(bx+lroundf(level.right*13),by+lroundf(level.down*13),4,color);
    } else g.drawFastHLine(bx-3,by,7,c.muted);
  }
}
inline void makeDemo(LaunchMotion::Trace &t,bool reference) {
  t={};t.fused=t.stationaryBias=true;t.quality=LaunchMotion::Quality::Valid;t.count=LaunchMotion::Trace::POINTS;t.number=reference?1:2;
  t.rpm=reference?6000:6500;t.durationMs=reference?400:450;t.gravity[2]=1;
  for(unsigned i=0;i<t.count;++i) {
    auto &p=t.points[i];p.ms=-500+int(i*1250/(t.count-1));
    const float s=std::max(0.0f,std::min(1.0f,p.ms/float(t.durationMs)));
    LaunchMotion::Quaternion q;const float rate[3]={reference?30.0f:45.0f,reference?65.0f:85.0f,reference?10.0f:30.0f};
    q.integrate(rate,s*t.durationMs*.001f);
    p.q[0]=lroundf(q.w*16384);p.q[1]=lroundf(q.x*16384);p.q[2]=lroundf(q.y*16384);p.q[3]=lroundf(q.z*16384);
    if(p.ms>0 && p.ms<int(t.durationMs))for(unsigned j=0;j<3;++j){p.g[j]=lroundf(rate[j]*10);p.a[j]=lroundf(std::sin(s*3.14159265f)*(reference?900:1300)*(j==0?1:.3f));}
  }
  LaunchMotion::recordLevels(t);
}
}
