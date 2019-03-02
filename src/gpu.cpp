#include <iostream>
#include "constants.h"
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
    {255, 255, 255, 255},
    {128, 128, 128, 255},
    {100, 100, 100, 255},
    {0, 0, 0, 255},
}};
Gpu::Gpu(Memory& memory, std::shared_ptr<IRenderer> renderer)
    : memory{&memory},
      renderer{std::move(renderer)},
      background_texture{
          this->renderer->create_texture(SCREEN_WIDTH, SCREEN_HEIGHT, false)},
      sprite_texture{
          this->renderer->create_texture(SCREEN_WIDTH, SCREEN_HEIGHT, true)},
      background_framebuffer(DISPLAY_SIZE),
      sprite_framebuffer(DISPLAY_SIZE),
      background_colors(COLORS),
      sprite_colors{SPRITE_COLORS, SPRITE_COLORS} {}

u8 Gpu::get_scx() const {
  return memory->at(0xff43);
}

u8 Gpu::get_scy() const {
  return memory->at(0xff42);
}

void Gpu::render_pixel(std::vector<Pixel>& pixels,
                       const u8 byte1,
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
    pixels[160 * screen_y + screen_x] = colors.at(color_index);
  }
}

void Gpu::render_sprites(int scanline) {
  const Registers::Lcdc lcdc{memory->at(Registers::Lcdc::Address)};
  auto sprite_attribs = memory->get_sprite_attributes();

  int sprite_height;

  switch (lcdc.sprite_size()) {
    case Registers::Lcdc::SpriteMode::EightByEight:
      sprite_height = 8;
      break;
    case Registers::Lcdc::SpriteMode::EightBySixteen:
      sprite_height = 16;
      break;
  }

  for (const auto sprite_attrib : sprite_attribs) {
    if (sprite_attrib.x <= 0 || sprite_attrib.x >= 168 ||
        sprite_attrib.y <= 0 || sprite_attrib.y >= 160) {
      continue;
    }

    const auto& colors = sprite_colors.at(sprite_attrib.palette_number());

    const int adjusted_y = sprite_attrib.y - 16;

    if (scanline >= adjusted_y && scanline < adjusted_y + sprite_height) {
      // The scanline's Y value relative to the sprite's
      const int sprite_y = scanline - adjusted_y;
      const int sprite_offset =
          sprite_attrib.flip_y() ? sprite_height - sprite_y - 1 : sprite_y;

      // Get the tile at the sprite's index offset by the scanline relative
      // to the sprite's Y
      const int tile_addr =
          VRAM_START + (16 * sprite_attrib.tile_index) + (2 * (sprite_offset));

      const u8 byte1 = memory->at(tile_addr);
      const u8 byte2 = memory->at(tile_addr + 1);

      const int y = adjusted_y + sprite_y;
      const int x = sprite_attrib.x - 8;

      for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
        const int flipped_pixel_x =
            sprite_attrib.flip_x() ? 7 - pixel_x : pixel_x;
        render_pixel(sprite_framebuffer, byte1, byte2, flipped_pixel_x,
                     x + pixel_x, y, colors);
      }
    }
  }
}

void Gpu::render_background(int scanline) {
  const Registers::Lcdc lcdc{memory->at(Registers::Lcdc::Address)};

  const u8 scx = get_scx();
  const u8 scy = get_scy();

  if (!lcdc.bg_on()) {
    return;
  }

  nonstd::span<const u8> tile_map_range =
      memory->get_range(lcdc.bg_tile_map_range());

  const u16 tile_data_base = lcdc.bg_tile_data_base();

  const bool is_signed = lcdc.is_tile_map_signed();

  const int scanline_tile_row = (scanline + scy) / 8;
  const u16 tile_scroll_offset = 32 * scanline_tile_row;

  const int tile_y = ((scanline + scy) % 8);

  const int tile_map_range_size = tile_map_range.size();

  for (int x = 0; x < 160; ++x) {
    int pixel_x = x + scx;
    const u16 tile_index = (pixel_x / 8);
    u16 offset_tile_index = (tile_scroll_offset + (tile_index % 32));

    if (offset_tile_index > tile_map_range_size - 1) {
      offset_tile_index -= tile_map_range_size;
    }

    const u8 tile = tile_map_range.at(offset_tile_index);

    u16 tile_addr;

    if (is_signed) {
      int16_t signed_index = static_cast<s8>(tile);
      tile_addr = tile_data_base + (16 * (signed_index + 128));
    } else {
      tile_addr = tile_data_base + (16 * (tile));
    }

    tile_addr += 2 * tile_y;

    const int screen_x = x;
    const u8 byte1 = memory->at(tile_addr);
    const u8 byte2 = memory->at(tile_addr + 1);

    render_pixel(background_framebuffer, byte1, byte2, pixel_x % 8, screen_x,
                 scanline, background_colors);
  }
}

std::array<Pixel, 4> Gpu::generate_colors(Palette palette, bool is_sprite) {
  const auto& base_colors = is_sprite ? SPRITE_COLORS : COLORS;
  std::array<Pixel, 4> colors;
  std::generate(colors.begin(), colors.end(),
                [palette, base_colors, i = 0]() mutable {
                  return base_colors.at(palette.get_color(i++));
                });
  colors[0].a = 0;
  return colors;
}

void Gpu::compute_background_palette(u8 palette) {
  background_colors = generate_colors({palette});
}

void Gpu::compute_sprite_palette(int palette_number, u8 palette) {
  sprite_colors.at(palette_number) = generate_colors({palette}, true);
}

void Gpu::render_scanline(int scanline) {
  render_background(scanline);
  render_sprites(scanline);
}

void Gpu::render() {
  // renderer->clear();
  renderer->draw_pixels(background_texture, background_framebuffer);
  renderer->draw_pixels(sprite_texture, sprite_framebuffer);
  // renderer->present();
  std::fill(sprite_framebuffer.begin(), sprite_framebuffer.end(),
            Pixel{0, 0, 0, 0});
}
}  // namespace gb
