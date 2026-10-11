#include "../LaunchLabMini/nvs_budget.h"
#include "../LaunchLabMini/practice_history.h"
#include "../LaunchLabMini/launch_motion.h"
#include "../LaunchLabMini/power_diagnostics.h"
#include <cassert>
#include <iostream>

int main() {
  // Synthetic capacity boundary cases use the encoded production image sizes.
  static_assert(NvsBudget::blobEntries(PracticeHistory::IMAGE_BYTES)==87);
  static_assert(NvsBudget::blobEntries(sizeof(PowerDiagnostics::Image))==43);
  static_assert(NvsBudget::blobEntries(LaunchMotion::IMAGE_BYTES)==39);
  static_assert(NvsBudget::blobEntries(1)==4 && NvsBudget::blobEntries(32)==4 && NvsBudget::blobEntries(33)==5);
  static_assert(NvsBudget::alternatingStoreEntries(PracticeHistory::IMAGE_BYTES)==261);
  constexpr unsigned available=160;
  assert(NvsBudget::alternatingStoreEntries(PracticeHistory::IMAGE_BYTES)>available);
  assert(NvsBudget::alternatingStoreEntries(LaunchMotion::IMAGE_BYTES)<=available);
  assert(NvsBudget::headroom(available,PracticeHistory::IMAGE_BYTES)==73);
  assert(NvsBudget::headroom(82,PracticeHistory::IMAGE_BYTES)<0);
  std::cout<<"PASS: synthetic NVS entry arithmetic, alternating-store capacity and history rewrite headroom\n";
}
