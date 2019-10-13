#include "hardware_thread.h"
#include <chrono>
#include <iostream>
#include "gba/cpu.h"
#include "gba/emulator.h"
#include "gba/mmu.h"

namespace gb::advance {
void HardwareThread::run() {
  u32 breakpoint_addr = 0;
  u32 watchpoint_addr = 0;

  bool execute = false;

  bool run = true;
  m_hardware.mmu->set_write_handler(
      [&watchpoint_addr, &execute](u32 addr, [[maybe_unused]] u32 value) {
        if (addr == watchpoint_addr) {
          execute = false;
        }
      });

  Cpu::Debugger debugger{[&execute] { execute = false; }};
  m_hardware.cpu->set_debugger(debugger);

  while (run) {
    if (!m_events.empty()) {
      std::lock_guard<std::mutex> lock{m_events_mutex};
      for (const Event& event : m_events) {
        std::visit(
            Overloaded{[&execute](SetExecute e) { execute = e.value; },
                       [&breakpoint_addr](SetBreakpoint e) {
                         fmt::printf("%08x\n", e.addr);
                         breakpoint_addr = e.addr;
                       },
                       [&watchpoint_addr](SetWatchpoint e) {
                         watchpoint_addr = e.addr;
                       },
                       [&run]([[maybe_unused]] Quit quit) { run = false; }},
            event);
      }
      m_events.clear();
    }

    if (execute) {
      try {
        auto prev_time = std::chrono::high_resolution_clock::now();

        while (execute && !execute_hardware(m_hardware)) {
#if 0
          for (int i = 0; i < 16; ++i) {
            fmt::printf("R%d: %08x ", i,
                        m_hardware.cpu->reg(static_cast<Register>(i)));
          }
          const auto program_status = m_hardware.cpu->program_status();
          fmt::printf("C:%d Z:%d V:%d N:%d\n",
                      static_cast<u32>(program_status.carry()),
                      static_cast<u32>(program_status.zero()),
                      static_cast<u32>(program_status.overflow()),
                      static_cast<u32>(program_status.negative()));
#endif
          if (m_hardware.cpu->reg(gb::advance::Register::R15) -
                  m_hardware.cpu->prefetch_offset() ==
              breakpoint_addr) {
            execute = false;
            break;
          }
        }

        auto time = std::chrono::high_resolution_clock::now() - prev_time;
        // auto ms =
        // std::chrono::duration_cast<std::chrono::milliseconds>(time);
        auto ms = std::chrono::duration_cast<
            std::chrono::duration<double, std::milli>>(time);
        fmt::print("fps: {}\r", (16.66 / ms.count()) * 60.0);
        wait_for_vsync();

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
