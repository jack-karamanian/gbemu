#pragma once

namespace gb {

struct SetStartingVolumeCommand {
  int starting_volume = 0;
};

struct SetIncreaseVolumeCommand {
  bool increase_volume = false;
};

struct SetPeriodCommand {
  int period = 0;
};

class EnvelopeMod {
  int period = 0;
  int timer = 0;
  int starting_volume = 0;
  int volume = 0;
  bool increase_volume = false;

 public:
  void enable();

  void set_starting_volume(int value) { starting_volume = value; }

  void set_increase_volume(bool value) { increase_volume = value; }

  void set_period(int value) { period = value == 0 ? 8 : value; }

  void clock(int step);

  float update(float input_volume) const;

  void dispatch(SetStartingVolumeCommand command) {
    set_starting_volume(command.starting_volume);
  }

  void dispatch(SetIncreaseVolumeCommand command) {
    set_increase_volume(command.increase_volume);
  }

  void dispatch(SetPeriodCommand command) { set_period(command.period); }
};
}  // namespace gb
