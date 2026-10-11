#pragma once
#include <cmath>
// Synthetic timing/pose regression: 83 ms optical span, irregular context
// timestamps and changing tilt/yaw. Contains no device or practice export.
// Historical function name is retained for the playback harness interface.
inline LaunchMotion::Trace savedTiltLaunch96() {
  LaunchMotion::Trace t;t.quality=LaunchMotion::Quality::Valid;t.fused=true;
  t.number=1;t.rpm=5000;t.durationMs=83;t.count=48;t.gravity[2]=1;
  for(unsigned i=0;i<t.count;++i) {
    auto &p=t.points[i];
    p.ms=i<12?-500+int(i*500/12):i<36?int((i-12)*83/23):83+int((i-35)*250/12);
    const float roll=0.4f*std::sin(float(i)*0.17f),yaw=float(i)*0.016f;
    const float cr=std::cos(roll/2),sr=std::sin(roll/2),cy=std::cos(yaw/2),sy=std::sin(yaw/2);
    const float q[4]={cr*cy,sr*cy,sr*sy,cr*sy};
    for(unsigned j=0;j<4;++j)p.q[j]=int16_t(std::lround(q[j]*16384));
  }
  LaunchMotion::recordLevels(t);return t;
}
