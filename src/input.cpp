#include "input.h"

namespace gb {

bool Input::update() {
  bool request_interrupt = false;
  const u8 input_selector = input_state & 0x30;
  const bool select_buttons{!(input_selector & 0x20)};
  const bool select_dpad{!(input_selector & 0x10)};

  if (select_buttons || select_dpad) {
    u8 button_bits{0};

    if (select_buttons) {
      button_bits = get_bits(start_set, select_set, b_set, a_set);
    }
    if (select_dpad) {
      button_bits = get_bits(down_set, up_set, left_set, right_set);
    }

    button_bits = (~button_bits) & 0x0f;

    // If any of the buttons changed
    if ((input_state & 0x0f) ^ button_bits) {
      // Check each button to see if it went from 1 -> 0
      for (int i = 0; i < 4; i++) {
        u8 mask = 0x1 << i;
        if ((input_state & mask) && !(button_bits & mask)) {
          request_interrupt = true;
          break;
        }
      }
    }

    input_state = 0xc0 | input_selector | button_bits;
  } else {
    input_state = 0xff;
  }
  return request_interrupt;
}
}  // namespace gb
