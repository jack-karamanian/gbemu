#include <vector>
#include "gpu.h"
#include "memory.h"

#define VRAM_START 0x8000
#define VRAM_END 0x97FF

namespace gb {
static const Pixel COLORS[4] = {
    {0, 0, 0},
    {64, 64, 64},
    {128, 128, 128},
    {255, 255, 255},
};
Gpu::Gpu(const System* const system, SDL_Renderer* const renderer)
    : system{system},
      renderer{renderer},
      screen_texture{SDL_CreateTexture(renderer,
                                       SDL_PIXELFORMAT_RGB888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       160,
                                       144)} {}

Gpu::~Gpu() {
  SDL_DestroyTexture(screen_texture);
}

void Gpu::render() {
  Pixel* texture_pixels = nullptr;
  int pitch = 0;
  SDL_LockTexture(screen_texture, nullptr, (void**)&texture_pixels, &pitch);
  for (int i = VRAM_START; i < VRAM_END; i += 2) {
    const int pixel_index = ((i % 16) * 160);
    const u8& byte1 = system->memory->memory[i];
    const u8& byte2 = system->memory->memory[i + 1];
    for (int shift = 7; i >= 0; i--) {
      const u8 mask = (0x1 << shift);
      const u8 low_bit = (byte1 & mask) >> shift;
      const u8 high_bit = (byte2 & mask) >> shift;

      const u8 color_index = (high_bit << 1) | low_bit;
      texture_pixels[pixel_index + (7 - shift)] = COLORS[color_index];
    }
  }
}
}  // namespace gb
