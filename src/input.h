#pragma once
#include "memory.h"
#include "utils.h"

namespace gb {
class Input {
  Memory* memory;
  bool start_set{false};
  bool select_set{false};
  bool a_set{false};
  bool b_set{false};
  bool up_set{false};
  bool down_set{false};
  bool left_set{false};
  bool right_set{false};

 public:
  Input(Memory&);
  bool update();
  void set_start(bool start) { start_set = start; }
  void set_select(bool select) { select_set = select; }
  void set_a(bool a) { a_set = a; }
  void set_b(bool b) { b_set = b; }
  void set_up(bool up) { up_set = up; }
  void set_down(bool down) { down_set = down; }
  void set_left(bool left) { left_set = left; }
  void set_right(bool right) { right_set = right; }
};
}  // namespace gb
