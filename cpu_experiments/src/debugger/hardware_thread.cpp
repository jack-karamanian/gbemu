#include "hardware_thread.h"
#include <chrono>
#include <stdexcept>
#include "gba/cpu.h"
#include "gba/emulator.h"
#include "gba/mmu.h"

namespace gb::advance {
HardwareThread::HardwareThread(Hardware hardware) : m_hardware{hardware} {
  m_hardware.cpu->set_debugger(Cpu::Debugger{[this] { execute = false; }});
  m_hardware.mmu->set_write_handler(
      [this](u32 addr, [[maybe_unused]] u8 value) {
        if (addr == m_watchpoint_addr) {
          execute = false;
        }
      });
}

void HardwareThread::handle_event(Event event) {
  std::visit(
      Overloaded{[this](SetExecute e) { execute = e.value; },
                 [this](SetBreakpoint e) { m_breakpoint_addr = e.addr; },
                 [this](SetWatchpoint e) { m_watchpoint_addr = e.addr; }},
      event);
}

void HardwareThread::run_frame() {
  bool draw_frame = false;
  const auto prev_time = std::chrono::high_resolution_clock::now();
  while (execute && !draw_frame) {
    const u32 pc =
        m_hardware.cpu->reg(Register::R15) - m_hardware.cpu->prefetch_offset();
    if (pc == m_breakpoint_addr) {
      execute = false;
      break;
    }
    try {
      draw_frame = execute_hardware(m_hardware);
      Cpu* cpu = m_hardware.cpu;
#if 0
      for (int i = 0; i < 16; ++i) {
        fmt::printf("R%d: %08x ", i, cpu->reg(static_cast<Register>(i)));
      }
      const ProgramStatus& program_status = cpu->program_status();
      fmt::printf("C:%d Z:%d V:%d N:%d\n", program_status.carry(),
                  program_status.zero(), program_status.overflow(),
                  program_status.negative());
#endif
    } catch (std::exception& e) {
      fmt::print("{}\n", e.what());
      execute = false;
    }
  }
  const auto time = std::chrono::high_resolution_clock::now() - prev_time;
  const auto ms =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
          time);
  frametime = ms.count();
  framerate = (16.66 / frametime) * 60.0;
  // fmt::print("fps: {} time: {}\r", framerate, frametime);
}
#if 0
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
    {
      std::lock_guard<std::mutex> lock{m_events_mutex};
      if (!m_events.empty()) {
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
    }

    if (execute) {
      try {
        auto prev_time = std::chrono::high_resolution_clock::now();

        {
          {
            std::lock_guard lock{frontend_mutex};
            while (execute && !execute_hardware(m_hardware)) {
              if (m_hardware.cpu->reg(gb::advance::Register::R15) -
                      m_hardware.cpu->prefetch_offset() ==
                  breakpoint_addr) {
                execute = false;
                break;
              }
            }
          }

          auto time = std::chrono::high_resolution_clock::now() - prev_time;
          auto ms = std::chrono::duration_cast<
              std::chrono::duration<double, std::milli>>(time);
          fmt::print("fps: {} time: {}\r", (16.66 / ms.count()) * 60.0,
                     ms.count());
          using namespace std::literals;
          std::this_thread::sleep_for(
              16ms -
              std::chrono::duration_cast<std::chrono::milliseconds>(time));
        }

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
#endif
}  // namespace gb::advance
