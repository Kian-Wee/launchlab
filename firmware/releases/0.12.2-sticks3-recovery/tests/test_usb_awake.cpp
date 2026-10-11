#include "usb_awake.h"
#include "power_status.h"
#include <cassert>
#include <cstring>
#include <iostream>

static void host(UsbHostAwake &u,uint32_t first=1000){
  u.sampleHost(first,true);assert(u.inhibited(first) && !u.remembered());
  u.sampleHost(first+UsbHostAwake::HOST_POLL_MS,true);assert(u.remembered());
}
int main(){
  // The IDF connection monitor starts optimistically true without a host.
  UsbHostAwake boot;boot.sampleHost(0,true);boot.samplePower(0,true,1);
  assert(boot.inhibited(0) && !boot.remembered());
  boot.sampleHost(250,false);assert(!boot.remembered() && !boot.inhibited(250));
  for(uint32_t t=500;t<300000;t+=250){boot.sampleHost(t,false);assert(!boot.powerPollDue(t) && !boot.inhibited(t));}
  UsbHostAwake charger;
  for(uint32_t t=0;t<300000;t+=250){charger.sampleHost(t,false);assert(!charger.powerPollDue(t) && !charger.remembered() && !charger.inhibited(t));}
  std::cout<<"PASS: startup optimistic SOF cannot latch a charger; charger never requests power polling\n";

  // No CDC/Serial boolean or reader participates: SOF alone inhibits immediately.
  UsbHostAwake u;assert(u.hostPollDue(1000));host(u);
  assert(!u.hostPollDue(1499) && u.hostPollDue(1500));
  u.samplePower(1250,true,5); // VIN and battery can coexist.
  assert(!u.powerPollDue(2249) && u.powerPollDue(2250));
  u.sampleHost(1500,false);assert(u.inhibited(1500));
  for(uint32_t t=2250;t<302250;t+=1000){u.samplePower(t,true,5);assert(u.inhibited(t));}
  assert(!std::strcmp(u.reason(301250),"host_seen_vin_present"));
  std::cout<<"PASS: idle host without serial reader; powered suspended host stays inhibited beyond inactivity timeout\n";

  u.samplePower(302250,true,4);assert(!u.remembered() && !u.inhibited(302250));
  assert(!u.powerPollDue(400000));host(u,400000);assert(u.inhibited(400250));
  u.samplePower(400250,true,1);u.sampleHost(400500,false);
  assert(u.inhibited(402250) && !u.inhibited(402251)); // Checked power freshness boundary.
  assert(u.remembered());u.samplePower(402500,true,1);assert(u.inhibited(402500));
  std::cout<<"PASS: confirmed detach clears identity; reconnect confirms again; stale power releases and checked recovery restores inhibition\n";

  UsbHostAwake unknown;host(unknown);
  unknown.sampleHost(1500,false);unknown.samplePower(1500,false,0);
  assert(!unknown.lastPowerReadOk() && !unknown.powerKnown(1500));
  assert(unknown.inhibited(3249) && !unknown.inhibited(3250) && unknown.remembered());
  assert(!unknown.powerPollDue(2499) && unknown.powerPollDue(2500));
  unknown.samplePower(2500,false,0);assert(!unknown.powerPollDue(3499));
  unknown.samplePower(3500,true,1);assert(unknown.inhibited(3500));
  unknown.samplePower(4500,false,0);assert(unknown.powerKnown(4500) && unknown.inhibited(4500));
  unknown.samplePower(5500,false,0);assert(unknown.inhibited(5500) && !unknown.inhibited(5501));
  unknown.samplePower(6500,true,0);assert(!unknown.remembered());
  std::cout<<"PASS: failed reads consume bounded poll budget, preserve known power until stale, expire grace, retain evidence for recovery\n";

  UsbHostAwake active;host(active);active.samplePower(1250,false,0);
  active.sampleHost(3600000,true);assert(active.inhibited(3600000));
  assert(!std::strcmp(active.reason(3600000),"usb_host"));
  UsbHostAwake wrap;const uint32_t base=0xffffff00u;host(wrap,base);
  wrap.samplePower(base+250,true,1);wrap.sampleHost(base+500,false);
  assert(wrap.inhibited(base+2200));assert(!wrap.inhibited(base+2251));
  wrap.samplePower(base+2300,true,1);assert(wrap.inhibited(base+2300));
  wrap.samplePower(base+3300,true,0);assert(!wrap.inhibited(base+3300) && !wrap.remembered());
  InactivityTimer timer;assert(timer.setMinutes(7));timer.touch(base);
  assert(!timer.due(base+timer.timeout()-1) && timer.due(base+timer.timeout()));
  assert(timer.minutes()==7 && !InactivityTimer::validMinutes(0));
  std::cout<<"PASS: current SOF authoritative during I2C failure; timers and power freshness survive millis wrap; stored timeout semantics unchanged\n";
}
