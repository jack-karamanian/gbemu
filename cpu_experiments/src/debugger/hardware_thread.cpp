#include <iostream>
#include "gba/cpu.h"
#include "gba/emulator.h"
#include "hardware_thread.h"

namespace gb::advance {
void HardwareThread::run() {
  gb::u32 breakpoint_addr = 0;
  bool execute = false;

  bool run = true;
  while (run) {
    {
      std::lock_guard<std::mutex> lock{m_events_mutex};

      for (const Event& event : m_events) {
        std::visit(
            [&execute, &breakpoint_addr, &run](auto&& e) {
              using T = std::decay_t<decltype(e)>;

              if constexpr (std::is_same_v<T, SetExecute>) {
                execute = e.value;
              } else if constexpr (std::is_same_v<T, SetBreakpoint>) {
                breakpoint_addr = e.addr;
              } else if constexpr (std::is_same_v<T, Quit>) {
                run = false;
              }
            },
            event);
      }
      m_events.clear();
    }

    if (m_hardware.cpu->reg(gb::advance::Register::R15) -
            m_hardware.cpu->prefetch_offset() ==
        breakpoint_addr) {
      execute = false;
    }

    if (execute) {
      try {
        execute_hardware(m_hardware);
      } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        execute = false;
      }
    } else {
      using namespace std::literals;
      std::this_thread::sleep_for(1ms);
    }
  }
}
}  // namespace gb::advance
