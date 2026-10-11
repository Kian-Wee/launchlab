#include "../LaunchLabMini/wake_ui_state.h"
#include <cassert>
#include <iostream>
int main() {
  for(unsigned mode=0;mode<2;++mode)for(unsigned view=0;view<2;++view)
    for(unsigned page=0;page<7;++page)for(unsigned history=0;history<2;++history) {
      WakeUi::State state{bool(mode),bool(history),uint8_t(view),uint8_t(page)},out;
      assert(WakeUi::decode(WakeUi::encode(state),6,out));
      assert(out.tournament==state.tournament && out.tournamentView==view && out.historyView==page);
      assert(out.history==(state.history && !state.tournament));
    }
  WakeUi::State out{true,false,1,3};
  for(uint32_t corrupt:{0u,0xFFFFFFFFu,WakeUi::MAGIC|0x80,WakeUi::MAGIC|4,WakeUi::MAGIC|6,
      WakeUi::MAGIC|0x70,WakeUi::MAGIC|9,0x4C570200u}) {
    assert(!WakeUi::decode(corrupt,6,out));assert(out.tournament && out.tournamentView==1);
  }
  assert(!WakeUi::decode(WakeUi::encode({false,true,0,6}),5,out));
  std::cout<<"PASS: all normal/tournament views and pages round-trip; corrupt, conflicting and incompatible records rejected\n";
}
