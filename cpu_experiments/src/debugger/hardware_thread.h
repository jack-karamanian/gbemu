#pragma once
#include <mutex>
#include <thread>
#include <variant>
#include <vector>
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

struct Quit {};

using Event = std::variant<SetExecute, SetBreakpoint, SetWatchpoint, Quit>;

class HardwareThread {
 public:
  HardwareThread(Hardware hardware)
      : m_hardware{hardware}, m_hardware_thread{[this] { run(); }} {}

  HardwareThread(HardwareThread&&) noexcept = delete;
  HardwareThread& operator=(HardwareThread&&) noexcept = delete;

  HardwareThread(const HardwareThread&) = delete;
  HardwareThread& operator=(const HardwareThread&) = delete;

  void push_event(Event event) {
    std::lock_guard<std::mutex> lock{m_events_mutex};
    m_events.emplace_back(event);
  }

  ~HardwareThread() { m_hardware_thread.join(); }

 private:
  void run();

  std::vector<Event> m_events;
  Hardware m_hardware;
  std::mutex m_events_mutex;
  std::thread m_hardware_thread;
};
}  // namespace gb::advance
