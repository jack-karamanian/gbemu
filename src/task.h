#pragma once

class Task {
 public:
  explicit Task(int ticks) : interval{ticks}, counter{0} {}

  template <typename Func>
  void run(int ticks, Func&& callback) {
    counter += ticks;
    if (counter >= interval) {
      counter -= interval;
      callback();
    }
  }

 private:
  int interval;
  int counter;
};
