#include "volume_shift_mod.h"

namespace gb {
void VolumeShiftMod::set_volume_shift(int code) {
  switch (code) {
    case 0:
      volume_shift = 4;
      break;
    case 1:
      volume_shift = 0;
      break;
    case 2:
      volume_shift = 1;
      break;
    case 3:
      volume_shift = 2;
      break;
  }
}
}  // namespace gb
