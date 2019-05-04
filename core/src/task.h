#pragma once
#include <functional>
#include <optional>
#include <type_traits>

class Task {
 public:
  explicit Task(int ticks) : interval{ticks}, counter{0} {}
  explicit Task(int ticks, int start_ahead)
      : interval{ticks}, counter{start_ahead} {}

  template <typename Func>
  void run(int ticks, Func&& callback) {
    counter += ticks;
    if (counter >= interval) {
      const int total_ticks = counter;
      counter -= interval;
      if constexpr (std::is_invocable_v<Func, int>) {
        callback(total_ticks);
      } else {
        callback();
      }
    }
  }

 private:
  int interval;
  int counter;
};

class ManualTask {
 public:
  explicit ManualTask(int interval_ticks)
      : interval{interval_ticks}, counter{0} {}

  void advance(int ticks) { counter += ticks; }

  template <typename Func>
  bool for_each_cycle(Func func) {
    bool stop = false;
    while (!stop && counter >= interval) {
      counter -= interval;
      stop = func(++cycles);
    }
    return stop;
  }

 private:
  int interval;
  int counter;
  int cycles = 0;
};
