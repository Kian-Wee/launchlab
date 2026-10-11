#include "../LaunchLabMini/launch_feedback.h"
#include <cassert>
#include <iostream>
int main() {
  LaunchFeedback view;view.toggle(100,false);assert(!view.shown());
  view.show(200);assert(view.shown() && view.elapsed(1600)==1400 && !view.tick(5199));assert(view.tick(5200) && !view.shown() && view.elapsed(5200)==0);
  view.toggle(6000,true);assert(view.shown());view.toggle(6001,true);assert(!view.shown());
  view.show(7000);view.show(10000);assert(!view.tick(12000) && view.shown());assert(view.tick(15000));
  view.show(UINT32_MAX-1000);assert(view.elapsed(3998)==4999 && !view.tick(3998) && view.shown());assert(view.tick(3999));
  view.show(1);view.clear();assert(!view.tick(9000) && !view.shown());
  std::cout<<"PASS: automatic five-second feedback, manual recent/live toggle, absent result, new-pull restart and timer wrap\n";
}
