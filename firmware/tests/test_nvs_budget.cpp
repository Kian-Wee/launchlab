#include "../LaunchLabMini/nvs_budget.h"
#include "../LaunchLabMini/practice_history.h"
#include "../LaunchLabMini/launch_motion.h"
#include "../LaunchLabMini/power_diagnostics.h"
#include <cassert>
#include <iostream>

int main() {
  // Entry counts read back from a StickS3 NVS dump on 2026-10-10: history0/1
  // 2668 B took 86 entries in one chunk; the 1256 B power log took 43 in two.
  static_assert(NvsBudget::blobEntries(PracticeHistory::IMAGE_BYTES)==87);
  static_assert(NvsBudget::blobEntries(sizeof(PowerDiagnostics::Image))==43);
  static_assert(NvsBudget::blobEntries(LaunchMotion::IMAGE_BYTES)==39);
  static_assert(NvsBudget::blobEntries(1)==4 && NvsBudget::blobEntries(32)==4 && NvsBudget::blobEntries(33)==5);
  // The `y` scratch check needs two slots plus a rewrite in flight; the device
  // had 158 available entries beside 120 real launches, so it cannot run there.
  static_assert(NvsBudget::alternatingStoreEntries(PracticeHistory::IMAGE_BYTES)==261);
  assert(NvsBudget::alternatingStoreEntries(PracticeHistory::IMAGE_BYTES)>158);
  // The `z` motion check fits in the same space.
  assert(NvsBudget::alternatingStoreEntries(LaunchMotion::IMAGE_BYTES)<=158);
  // A real history save needs one rewrite's worth of free entries.
  assert(NvsBudget::headroom(158,PracticeHistory::IMAGE_BYTES)==71);
  assert(NvsBudget::headroom(82,PracticeHistory::IMAGE_BYTES)<0);
  std::cout<<"PASS: NVS budget matches device entry counts; y needs "
           <<NvsBudget::alternatingStoreEntries(PracticeHistory::IMAGE_BYTES)<<" entries, a history save "
           <<NvsBudget::blobEntries(PracticeHistory::IMAGE_BYTES)<<"\n";
}
