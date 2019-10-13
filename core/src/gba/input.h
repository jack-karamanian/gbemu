#pragma once
#include "utils.h"

namespace gb::advance {
class Input : public Integer<u16> {
 public:
  Input() : Integer::Integer{0x03ff} {}

  void write_byte([[maybe_unused]] unsigned int byte,
                  [[maybe_unused]] u8 value) const {
    // Disable writes
  }

  void set_a(bool set) { set_bit(0, !set); }

  void set_b(bool set) { set_bit(1, !set); }

  void set_select(bool set) { set_bit(2, !set); }

  void set_start(bool set) { set_bit(3, !set); }

  void set_right(bool set) { set_bit(4, !set); }

  void set_left(bool set) { set_bit(5, !set); }

  void set_up(bool set) { set_bit(6, !set); }

  void set_down(bool set) { set_bit(7, !set); }

  void set_r(bool set) { set_bit(8, !set); }

  void set_l(bool set) { set_bit(9, !set); }
};
}  // namespace gb::advance
