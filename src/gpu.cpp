#include "gpu.h"
#include "memory.h"
#include "renderer.h"

#define VRAM_START 0x8000
#define VRAM_END 0x97FF

namespace gb {
static constexpr std::array<Pixel, 4> COLORS = {{
    {0, 0, 0},
    {64, 64, 64},
    {128, 128, 128},
    {255, 255, 255},
}};
Gpu::Gpu(Memory& memory, std::unique_ptr<IRenderer> renderer)
    : memory{&memory}, renderer{std::move(renderer)} {}

void Gpu::render() {
  for (int i = VRAM_START; i < VRAM_END; i += 2) {
    const int pixel_index = ((i % 16) * 160);
    const u8& byte1 = memory->memory[i];
    const u8& byte2 = memory->memory[i + 1];
    for (int shift = 7; shift >= 0; --shift) {
      const u8 mask = (0x1 << shift);
      const u8 low_bit = (byte1 & mask) >> shift;
      const u8 high_bit = (byte2 & mask) >> shift;

      const u8 color_index = (high_bit << 1) | low_bit;
      pixels[pixel_index + (7 - shift)] = COLORS[color_index];
    }
  }

  renderer->draw_pixels(pixels.data());
}
}  // namespace gb
