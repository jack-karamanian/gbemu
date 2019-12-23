#pragma once
#include <variant>
#include "gba/hardware.h"
#include "types.h"

namespace gb::advance {
struct SetExecute {
  bool value;
};

struct SetBreakpoint {
  u32 addr;
};

struct SetWatchpoint {
  u32 addr;
};

using Event = std::variant<SetExecute, SetBreakpoint, SetWatchpoint>;

class HardwareThread {
 public:
  HardwareThread(Hardware hardware);

  void push_event(Event event) { handle_event(event); }

  void run_frame();

  double framerate = 0.0;
  double frametime = 0.0;
  bool execute = false;

 private:
  void handle_event(Event event);

  Hardware m_hardware;
  u32 m_breakpoint_addr = 0;
  u32 m_watchpoint_addr = 0;
};
}  // namespace gb::advance
