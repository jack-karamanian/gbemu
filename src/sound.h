#pragma once
#include <functional>
#include <vector>
#include "channel.h"
#include "noise_source.h"
#include "sound_mods/envelope_mod.h"
#include "sound_mods/length_mod.h"
#include "sound_mods/quiet_mod.h"
#include "sound_mods/volume_shift_mod.h"
#include "square_source.h"
#include "types.h"
#include "wave_source.h"

namespace gb {
class Memory;
struct OutputControl {
  bool square1 = false;
  bool square2 = false;
  bool wave = false;
  bool noise = false;

  int volume = 0;

  void set_enabled(u8 map) {
    square1 = (map & 0x1) != 0;
    square2 = (map & 0x2) != 0;
    wave = (map & 0x4) != 0;
    noise = (map & 0x8) != 0;
  }
};

struct AudioFrame {
  float square1_sample = 0.0f;
  float square2_sample = 0.0f;
  float wave_sample = 0.0f;
  float noise_sample = 0.0f;
};

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

  Channel<NoiseSource, LengthMod<64>, EnvelopeMod, QuietMod> noise_channel;

  OutputControl left_output;
  OutputControl right_output;

  std::vector<float> sample_buffer;

  SamplesCallback samples_ready_callback;

  float mix_samples(const AudioFrame& frame, const OutputControl& control);

 public:
  Sound(Memory& memory);
  void set_samples_ready_listener(SamplesCallback callback) {
    samples_ready_callback = std::move(callback);
  }

  void handle_memory_write(u16 addr, u8 value);

  void update(int ticks);
};
}  // namespace gb
