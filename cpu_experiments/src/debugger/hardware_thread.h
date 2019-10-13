#pragma once
#include <condition_variable>
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

  void signal_vsync() {
    std::lock_guard<std::mutex> lock{m_vsync_mutex};
    m_vsync_done = true;
    m_vsync_cv.notify_one();
  }

  ~HardwareThread() { m_hardware_thread.join(); }

 private:
  void run();

  void wait_for_vsync() {
    std::unique_lock<std::mutex> lock{m_vsync_mutex};
    m_vsync_cv.wait(lock, [this] { return m_vsync_done; });
    m_vsync_done = false;
  }

  std::vector<Event> m_events;
  Hardware m_hardware;
  std::mutex m_events_mutex;

  std::mutex m_vsync_mutex;
  std::condition_variable m_vsync_cv;

  bool m_vsync_done = false;

  std::thread m_hardware_thread;
};
}  // namespace gb::advance
