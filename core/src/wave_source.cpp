#include "wave_source.h"
namespace gb {
void WaveSource::enable() {
  enabled = true;
  timer = timer_base;
  wave_progress = 0;
}

u8 WaveSource::get_volume(int progress) const {
  const u8 sample = wave_buffer.at(progress / 2);
  return (progress % 2 == 0 ? (sample & 0xf0) >> 4 : sample & 0x0f);
}

}  // namespace gb
