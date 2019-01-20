#include <iostream>
#include "gpu.h"
#include "memory.h"
#include "renderer.h"
#include "sprite_attribute.h"

#define VRAM_START 0x8000
#define VRAM_END 0x97ff

#define OAM_START 0xfe00

namespace gb {
static constexpr std::array<Pixel, 4> COLORS = {{
    {0, 0, 0, 255},
    {64, 64, 64, 255},
    {128, 128, 128, 255},
    {255, 255, 255, 255},
}};
Gpu::Gpu(Memory& memory, std::unique_ptr<IRenderer> renderer)
    : memory{&memory}, renderer{std::move(renderer)} {}
Gpu::~Gpu() {
  std::cout << "Gpu Destructor" << std::endl;
}
void Gpu::dump_vram() {
  for (int i = VRAM_START; i < VRAM_END; i += 2) {
    const int textures_read = (i - VRAM_START) / 16;
    const int pixel_index = ((i % 16) * 160);
    int y = ((i / 2) % 8);  //+ (8 * textures_read);
    const u8& byte1 = memory->memory[i];
    const u8& byte2 = memory->memory[i + 1];
    for (int shift = 7; shift >= 0; --shift) {
      const u8 mask = (0x1 << shift);
      const u8 low_bit = (byte1 & mask) >> shift;
      const u8 high_bit = (byte2 & mask) >> shift;

      const u8 color_index = (high_bit << 1) | low_bit;
      int x = (7 - shift) + (8 * textures_read);
      // if (false && x > 160 - 1 || y > 144 - 1) {
      //  break;
      //}
      if (x > 160 - 1) {
        y += 8;
        x = 0;
      }
      pixels[144 * (x % 160) + (y % 144)] = COLORS.at(color_index);
    }
  }
}

void Gpu::render_sprites(int scanline) {
  for (int oam_attr = 0; oam_attr <= 0x9b; oam_attr += 4) {
    auto sprite_attrib =
        reinterpret_cast<const SpriteAttribute*>(memory->at(0xFE00 + oam_attr));

    if (sprite_attrib->x <= 0 || sprite_attrib->x >= 168 ||
        sprite_attrib->y <= 0 || sprite_attrib->y >= 160) {
      continue;
    }
    std::cout << "index: " << std::dec << +sprite_attrib->tile_index
              << std::endl;
    std::cout << "scanline: " << scanline << std::endl;
    std::cout << "y: " << std::hex << +sprite_attrib->y << std::endl;

    // The scanline's Y value relative to the sprite's
    const int sprite_y = scanline % sprite_attrib->y;

    if (scanline >= sprite_attrib->y && scanline < sprite_attrib->y + 8) {
      // Get the tile at the sprite's index offset by the scanline relative to
      // the sprite's Y
      const int tile_addr =
          VRAM_START + (16 * sprite_attrib->tile_index) + (2 * (sprite_y));

      auto tile = memory->at(tile_addr);
      std::cout << "tile_addr: " << tile_addr << std::endl;

      const u8 byte1 = tile[1];
      const u8 byte2 = tile[0];
      for (int shift = 7; shift >= 0; --shift) {
        const u8 mask = (0x1 << shift);
        const u8 low_bit = (byte1 & mask) >> shift;
        const u8 high_bit = (byte2 & mask) >> shift;

        const u8 color_index = (high_bit << 1) | low_bit;
        const int x = ((7 - shift) + sprite_attrib->x) - 8;
        const int y = scanline - 16;
        if (x < 160 && y >= 0) {
          pixels[144 * x + y] = COLORS.at(color_index);
        }
      }
    }
  }
}

void Gpu::render_scanline(int scanline) {
  std::cout << "scanlines :" << std::dec << scanline << std::endl;
  for (int i = 0; i < 160; ++i) {
    pixels[144 * i + scanline] = {0, 0, 0, 0};
  }
  render_sprites(scanline);
}

void Gpu::render() {
  std::cout << "draw_pixels " << std::boolalpha << (renderer.get() == nullptr)
            << std::endl;
  renderer->draw_pixels(pixels);
  // char c;
  // std::cin.read(&c, 1);
}
}  // namespace gb
