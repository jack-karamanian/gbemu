#pragma once
#include <functional>
#include <vector>
#include "square_channel.h"
#include "types.h"

namespace gb {
class Memory;
class Sound {
  using SamplesCallback = std::function<void(const std::vector<float>&)>;
  Memory* memory;
  int sequencer_ticks = 0;
  int sequencer_step = 0;
  int sample_ticks = 0;

  SquareChannel square1;
  SquareChannel square2;

  std::vector<float> sample_buffer;

  SamplesCallback samples_ready_callback;

 public:
  Sound(Memory& memory);
  void set_samples_ready_listener(SamplesCallback callback) {
    samples_ready_callback = std::move(callback);
  }

  void handle_memory_write(u16 addr, u8 value);

  void update(int ticks);
};
}  // namespace gb
