#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <vector>
#include "channel.h"
#include "constants.h"
#include "noise_source.h"
#include "sound_mods/envelope_mod.h"
#include "sound_mods/length_mod.h"
#include "sound_mods/quiet_mod.h"
#include "sound_mods/volume_shift_mod.h"
#include "square_source.h"
#include "task.h"
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
  u8 square1_sample = 0;
  u8 square2_sample = 0;
  u8 wave_sample = 0;
  u8 noise_sample = 0;
};

class Sound {
  using SamplesCallback = std::function<void(const std::vector<float>&)>;
  using SquareChannel = Channel<SquareSource, LengthMod<64>, EnvelopeMod>;

  Memory* memory;

  Task samples_task{CLOCK_FREQUENCY / SOUND_SAMPLE_FREQUENCY};
  Task sequencer_task{8192};

  int sequencer_step = 0;
  int sample_ticks = 0;

  SquareChannel square1;
  SquareChannel square2;

  Channel<WaveSource, LengthMod<256>, VolumeShiftMod> wave_channel;

  Channel<NoiseSource, LengthMod<64>, EnvelopeMod> noise_channel;

  OutputControl left_output;
  OutputControl right_output;

  std::vector<u8> noise_samples;
  std::vector<float> sample_buffer;

  SDL_AudioDeviceID audio_device;

  [[nodiscard]] float mix_samples(const AudioFrame& frame,
                                  const OutputControl& control) const;

  bool sound_power_on = false;

 public:
  Sound(Memory& memory, SDL_AudioDeviceID device);

  u8 handle_memory_read(u16 addr) const;
  void handle_memory_write(u16 addr, u8 value);

  void update(int ticks);
};
}  // namespace gb
