#include "../LaunchLabMini/practice_views.h"
#include "../LaunchLabMini/wake_ui_state.h"
#include <cassert>
#include <iostream>
int main() {
  using PracticeUI::View;
  const View pages[]={View::Recent,View::Pulls,View::Sessions,View::Battery,View::Settings};
  static_assert(unsigned(View::Settings)==6,"preserve released Settings wake value");
  View page=View::Recent;
  for(unsigned i=0;i<15;++i) {
    assert(page==pages[i%5]);
    assert(unsigned(page)!=4 && unsigned(page)!=5);
    page=PracticeUI::next(page);
  }
  assert(page==View::Recent);
  for(unsigned i=0;i<5;++i)assert(PracticeUI::restoredView(unsigned(pages[i]))==pages[i]);
  // The existing one-shot wake format accepts old pages, then navigation maps
  // retired Motion/Recreate to history; Settings must remain Settings.
  for(uint8_t oldPage:{4,5,6}) {
    const uint32_t saved=WakeUi::encode({false,true,0,oldPage});
    WakeUi::State state;
    assert(WakeUi::decode(saved,6,state) && state.history);
    assert(PracticeUI::restoredView(state.historyView)==(oldPage==6?View::Settings:View::Recent));
    assert(WakeUi::encode(state)==saved); // no mutation/migration of the saved format
  }
  assert(PracticeUI::next(View(4))==View::Recent);
  assert(PracticeUI::next(View(5))==View::Recent);
  assert(PracticeUI::restoredView(255)==View::Recent);
  std::cout<<"PASS: five-page cycle excludes retired pages; old Motion/Recreate wake markers fall back to history; Settings numeric compatibility retained\n";
}
