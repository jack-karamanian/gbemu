#include <iostream>
#include "gpu.h"
#include "memory.h"
#include "registers/lcdc.h"
#include "renderer.h"
#include "sprite_attribute.h"

#define VRAM_START 0x8000
#define VRAM_END 0x97ff

#define OAM_START 0xfe00

namespace gb {
static constexpr std::array<Pixel, 4> COLORS = {{
    {255, 255, 255, 255},
    {128, 128, 128, 255},
    {100, 100, 100, 255},
    {32, 32, 32, 255},
}};

static constexpr std::array<Pixel, 4> SPRITE_COLORS = {{
    {255, 255, 255, 0},
    {128, 128, 128, 255},
    {100, 100, 100, 255},
    {0, 0, 0, 0},
}};
Gpu::Gpu(Memory& memory, std::unique_ptr<IRenderer> renderer)
    : memory{&memory}, renderer{std::move(renderer)}, pixels(DISPLAY_SIZE) {}

u8 Gpu::get_scx() const {
  return memory->at(0xff43);
}

u8 Gpu::get_scy() const {
  return memory->at(0xff42);
}

void Gpu::render_pixel(const u8 byte1,
                       const u8 byte2,
                       const u8 pixel_x,
                       const int screen_x,
                       const int screen_y,
                       const std::array<Pixel, 4>& colors) {
  const u8 shift = 7 - pixel_x;
  const u8 mask = (0x1 << shift);
  const u8 low_bit = (byte1 & mask) >> shift;
  const u8 high_bit = (byte2 & mask) >> shift;

  const u8 color_index = (high_bit << 1) | low_bit;

  if (screen_x < 160 && screen_y >= 0) {
    pixels[144 * screen_x + screen_y] = colors.at(color_index);
  }
}

void Gpu::render_tile(const u8 byte1,
                      const u8 byte2,
                      const int screen_x,
                      const int screen_y,
                      const std::array<Pixel, 4>& colors) {
  for (int pixel_x = 0; pixel_x < 8; ++pixel_x) {
    render_pixel(byte1, byte2, pixel_x, screen_x, screen_y, colors);
  }
}

void Gpu::render_sprites(int scanline) {
  auto sprite_attribs = memory->get_sprite_attributes();

  for (const auto& sprite_attrib : sprite_attribs) {
    if (sprite_attrib.x <= 0 || sprite_attrib.x >= 168 ||
        sprite_attrib.y <= 0 || sprite_attrib.y >= 160) {
      continue;
    }

    // The scanline's Y value relative to the sprite's
    const int sprite_y = scanline % sprite_attrib.y;

    if (scanline >= sprite_attrib.y && scanline < sprite_attrib.y + 8) {
      // Get the tile at the sprite's index offset by the scanline relative to
      // the sprite's Y
      const int tile_addr =
          VRAM_START + (16 * sprite_attrib.tile_index) + (2 * (sprite_y));

      const u8 byte1 = memory->at(tile_addr);
      const u8 byte2 = memory->at(tile_addr + 1);

      const int y = scanline - 16;
      const int x = sprite_attrib.x - 8;

      for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
        const int xpos = sprite_attrib.flip_x() ? x - pixel_x : x + pixel_x;
        render_pixel(byte1, byte2, pixel_x, xpos, y, SPRITE_COLORS);
      }
    }
  }
}  // namespace gb

void Gpu::render_background(int scanline) {
  const Registers::Lcdc lcdc{memory->at(Registers::Lcdc::Address)};

  const u8 scx = get_scx();
  const u8 scy = get_scy();

  if (!lcdc.bg_on()) {
    // return;
  }

  nonstd::span<const u8> tile_map_range =
      memory->get_range(lcdc.bg_tile_map_range());

  const u16 tile_data_base = lcdc.bg_tile_data_base();

  const bool is_signed = lcdc.is_tile_map_signed();

  const int scanline_tile_row = (scanline + scy) / 8;
  const u16 tile_scroll_offset = 32 * scanline_tile_row;

  const int tile_y = ((scanline + scy) % 8);

  for (int x = 0; x < 160; ++x) {
    int pixel_x = x + scx;
    const u16 tile_index = (pixel_x / 8);
    const u16 offset_tile_index =
        (tile_scroll_offset + (tile_index % 32)) % tile_map_range.size();

    const u8 tile = tile_map_range.at(offset_tile_index);

    u16 tile_addr;

    if (is_signed) {
      int16_t signed_index = static_cast<s8>(tile);
      tile_addr = tile_data_base + (16 * (signed_index + 128));
    } else {
      tile_addr = tile_data_base + (16 * (tile));
    }

    tile_addr += 2 * tile_y;

    // const u8* tile_data = memory->at(tile_addr);

    const int screen_x = x;
    const u8 byte1 = memory->at(tile_addr);
    const u8 byte2 = memory->at(tile_addr + 1);

    render_pixel(byte1, byte2, pixel_x % 8, screen_x, scanline, COLORS);
  }
}

void Gpu::render_scanline(int scanline) {
  render_background(scanline);
  render_sprites(scanline);
}

void Gpu::render() {
  renderer->draw_pixels(pixels);
}
}  // namespace gb
