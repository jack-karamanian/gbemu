#include "wave_source.h"
namespace gb {
void WaveSource::enable() {
  enabled = true;
  timer = timer_base;
  wave_progress = 0;
}

u8 WaveSource::get_sample() {
  if (--timer <= 0) {
    timer = timer_base;
    wave_progress++;
  }

  if (wave_progress >= 32) {
    wave_progress = 0;
  }

  return wave_buffer.at(wave_progress / 2);
}

u8 WaveSource::update() {
  if (!enabled) {
    return 0;
  }

  u8 sample = get_sample();

  u8 volume = wave_progress % 2 == 0 ? (sample & 0xf0) >> 4 : sample & 0x0f;

  return volume;
}
}  // namespace gb