#pragma once
#include <functional>
#include <vector>
#include "channel.h"
#include "sound_mods/envelope_mod.h"
#include "sound_mods/length_mod.h"
#include "sound_mods/quiet_mod.h"
#include "sound_mods/volume_shift_mod.h"
#include "square_source.h"
#include "types.h"
#include "wave_source.h"

namespace gb {
class Memory;
class Sound {
  using SamplesCallback = std::function<void(const std::vector<float>&)>;
  using SquareChannel =
      Channel<SquareSource, LengthMod<64>, EnvelopeMod, QuietMod>;
  Memory* memory;
  int sequencer_ticks = 0;
  int sequencer_step = 0;
  int sample_ticks = 0;

  SquareChannel square1;
  SquareChannel square2;

  Channel<WaveSource, LengthMod<256>, VolumeShiftMod, QuietMod> wave_channel;

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
