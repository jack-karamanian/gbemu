#pragma once
#include "types.h"

namespace gb {
struct SetLengthEnabledCommand {
  bool enabled;
};

struct SetLengthCommand {
  int length;
};

template <int DefaultLength>
class LengthMod {
  bool enabled = true;
  int length_counter = 0;
  bool length_enabled = false;

 public:
  void enable() {
    enabled = true;
    if (length_enabled && length_counter <= 0) {
      length_counter = DefaultLength;
    }
  }

  u8 update(u8 volume) const { return enabled ? volume : 0; }

  void set_length(int length) { length_counter = DefaultLength - length; }

  void set_length_enabled(bool value) { length_enabled = value; }

  void clock(int step) {
    if (step % 2 == 0 && enabled && length_enabled && --length_counter <= 0) {
      length_counter = 0;
      enabled = false;
    }
  }

  void dispatch(SetLengthEnabledCommand command) {
    set_length_enabled(command.enabled);
  }

  void dispatch(SetLengthCommand command) { set_length(command.length); }
};
}  // namespace gb
