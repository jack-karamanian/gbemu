#pragma once
#include "types.h"

namespace gb {
struct VolumeShiftCommand {
  int code = 0;
};

class VolumeShiftMod {
  int volume_shift = 0;

 public:
  void set_volume_shift(int code);

  void enable() {}
  void clock(int step) { static_cast<void>(step); }

  u8 update(u8 volume) const { return volume >> volume_shift; }

  void dispatch(VolumeShiftCommand command) { set_volume_shift(command.code); }
};
}  // namespace gb
