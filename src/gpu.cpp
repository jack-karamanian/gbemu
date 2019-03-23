#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/combine.hpp>
#include <boost/range/irange.hpp>
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
static constexpr std::array<Color, 4> COLORS = {{
    {255, 255, 255, 255},
    {128, 128, 128, 255},
    {100, 100, 100, 255},
    {32, 32, 32, 255},
}};

static constexpr std::array<Color, 4> SPRITE_COLORS = {{
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
      background_framebuffer(DISPLAY_SIZE),
      background_colors(COLORS),
      sprite_colors{SPRITE_COLORS, SPRITE_COLORS},
      background_pixels(DISPLAY_SIZE) {}

u8 Gpu::get_scx() const {
  return memory->get_ram(0xff43);
}

u8 Gpu::get_scy() const {
  return memory->get_ram(0xff42);
}

std::pair<Color, u8> Gpu::render_pixel(
    const u8 byte1,
    const u8 byte2,
    const u8 pixel_x,
    const std::array<Color, 4>& colors) const {
  const u8 shift = 7 - pixel_x;
  const u8 mask = (0x1 << shift);
  const u8 low_bit = (byte1 & mask) >> shift;
  const u8 high_bit = (byte2 & mask) >> shift;

  const u8 color_index = (high_bit << 1) | low_bit;

  return {colors.at(color_index), color_index};
}

auto Gpu::render_sprites(int scanline) const {
  using namespace boost::adaptors;
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};
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

  auto sprite_pixels =
      sprite_attribs | filtered([](auto sprite_attrib) {
        return sprite_attrib.x > 0 && sprite_attrib.x < 168 &&
               sprite_attrib.y > 0 && sprite_attrib.y < 160;
      }) |
      filtered([scanline, sprite_height](auto sprite_attrib) {
        const int adjusted_y = sprite_attrib.y - 16;
        return scanline >= adjusted_y && scanline < adjusted_y + sprite_height;
      }) |
      transformed([this, scanline, sprite_height](auto sprite_attrib) {
        const int adjusted_y = sprite_attrib.y - 16;

        const auto& colors = sprite_colors.at(sprite_attrib.palette_number());

        // The scanline's Y value relative to the sprite's
        const int sprite_y = scanline - adjusted_y;
        const int sprite_offset =
            sprite_attrib.flip_y() ? sprite_height - sprite_y - 1 : sprite_y;

        // Get the tile at the sprite's index offset by the scanline relative
        // to the sprite's Y
        const int tile_addr = VRAM_START + (16 * sprite_attrib.tile_index) +
                              (2 * (sprite_offset));

        const u8 byte1 = memory->get_ram(tile_addr);
        const u8 byte2 = memory->get_ram(tile_addr + 1);

        const int y = adjusted_y + sprite_y;
        const int x = sprite_attrib.x - 8;

        // for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
        return boost::irange(0, 8) |
               transformed([this, &colors, sprite_attrib, x, y, byte1,
                            byte2](int pixel_x) {
                 const int flipped_pixel_x =
                     sprite_attrib.flip_x() ? 7 - pixel_x : pixel_x;
                 const int screen_x = x + pixel_x;
                 const auto [color, color_index] =
                     render_pixel(byte1, byte2, flipped_pixel_x, colors);
                 return Pixel{sprite_attrib.above_bg()
                                  ? BackgroundPosition::Above
                                  : BackgroundPosition::Below,
                              {screen_x, y},
                              color,
                              color_index};
               });
      });

  return sprite_pixels;

#if 0
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

      const u8 byte1 = memory->get_ram(tile_addr);
      const u8 byte2 = memory->get_ram(tile_addr + 1);

      const int y = adjusted_y + sprite_y;
      const int x = sprite_attrib.x - 8;

      for (int pixel_x = 0; pixel_x < 8; pixel_x++) {
        const int flipped_pixel_x =
            sprite_attrib.flip_x() ? 7 - pixel_x : pixel_x;
        const int screen_x = x + pixel_x;
        const auto [color, color_index] =
            render_pixel(byte1, byte2, flipped_pixel_x, colors);
      }
    }
  }
#endif
}

void render_tile_map(nonstd::span<const u8> tile_map) {
  std::vector<u8> tile_map_range(tile_map.begin(), tile_map.end());
}

auto Gpu::render_background(int scanline,
                            nonstd::span<const u8> tile_map_range,
                            u8 scx,
                            u8 scy,
                            int offset_x,
                            int offset_y) {
  /*
  nonstd::span<const u8> tile_map_range =
      memory->get_range(lcdc.bg_tile_map_range());
  */
  using namespace boost::range;
  using namespace boost::adaptors;

  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};

  const u16 tile_data_base = lcdc.bg_tile_data_base();

  const bool is_signed = lcdc.is_tile_map_signed();

  const int scanline_tile_row = (scanline + scy + offset_y) / 8;
  const u16 tile_scroll_offset = 32 * scanline_tile_row;

  const int tile_y = ((scanline + scy) % 8);

  const int tile_map_range_size = tile_map_range.size();

  const int screen_y = scanline + offset_y;

  auto tile_addrs =

      boost::irange(0, 160) | transformed([scx](int x) { return x + scx; }) |
      transformed([](int adjusted_x) { return adjusted_x / 8; }) |
      transformed([tile_scroll_offset](int tile_index) -> u16 {
        return tile_scroll_offset + (tile_index % 32);
      }) |
      transformed([tile_map_range_size](u16 tile_index) -> u16 {
        return tile_index > tile_map_range_size - 1
                   ? tile_index - tile_map_range_size
                   : tile_index;
      }) |
      transformed([tile_map_range](u16 tile_index) -> u8 {
        return tile_map_range[tile_index];
      }) |
      transformed([is_signed, tile_data_base, tile_y](u8 tile_num) {
        return (tile_data_base +
                16 *
                    (is_signed
                         ? static_cast<int16_t>(static_cast<s8>(tile_num)) + 128
                         : tile_num)) +
               2 * tile_y;
      });
  return boost::range::combine(boost::irange(0, 160), tile_addrs) |
         transformed([this, offset_x, scx, screen_y](auto&& pair) -> Pixel {
           const int x = boost::get<0>(pair);
           const u16 tile_addr = boost::get<1>(pair);

           const int screen_x = x + offset_x;
           const u8 byte1 = memory->get_ram(tile_addr);
           const u8 byte2 = memory->get_ram(tile_addr + 1);

           const auto [color, color_index] =
               render_pixel(byte1, byte2, (x + scx) % 8, background_colors);

           return Pixel{BackgroundPosition::None,
                        {screen_x, screen_y},
                        color,
                        color_index};
         });

#if 0
  for (int x = 0; x < 160; ++x) {
    const int pixel_x = x + scx;
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

    const int screen_x = x + offset_x;
    const u8 byte1 = memory->get_ram(tile_addr);
    const u8 byte2 = memory->get_ram(tile_addr + 1);

    const auto [color, color_index] =
        render_pixel(byte1, byte2, pixel_x % 8, background_colors);

    PixelGroup& pixel_group = pixels[SCREEN_WIDTH * screen_y + screen_x];

    Pixel& pixel = is_window ? pixel_group.window : pixel_group.background;

    pixel = Pixel {}
  }
#endif
}

std::array<Color, 4> Gpu::generate_colors(Palette palette, bool is_sprite) {
  const auto& base_colors = is_sprite ? SPRITE_COLORS : COLORS;
  std::array<Color, 4> colors;
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

static Uint64 prev_time = 0;
static Uint64 time = 0;
static Uint64 avg_count = 1;

void Gpu::render_scanline(int scanline) {
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};
  const u16 window_y = memory->get_ram(0xff4a);
  // prev_time = SDL_GetPerformanceCounter();
  if (lcdc.window_on() && scanline >= window_y) {
    const u16 window_x = memory->get_ram(0xff4b) - 7;
    const auto tile_map_range = memory->get_range(lcdc.window_tile_map_range());
    auto window =
        render_background(scanline, tile_map_range, 0, 0, 0, -window_y);
    std::copy(window.begin(), window.end(),
              background_pixels.begin() + SCREEN_WIDTH * scanline);
    /*
    for (Pixel&& pixel : window) {
      color_index_cache[pixel.screen_pos.x] = pixel.color_index;
      background_framebuffer[SCREEN_WIDTH * (scanline) + pixel.screen_pos.x] =
          pixel.color;
    }
    */
  } else if (lcdc.bg_on()) {
    const auto tile_map_range = memory->get_range(lcdc.bg_tile_map_range());

    auto background =
        render_background(scanline, tile_map_range, get_scx(), get_scy(), 0, 0);

    std::copy(background.begin(), background.end(),
              background_pixels.begin() + SCREEN_WIDTH * scanline);

    /*
    for (Pixel& pixel : background_pixels) {
      color_index_cache[pixel.screen_pos.x] = pixel.color_index;
    }
    for (Pixel& pixel : background_pixels) {
      background_framebuffer[SCREEN_WIDTH * pixel.screen_pos.y +
                             pixel.screen_pos.x] = pixel.color;
    }
    */

    /*
  for (const auto& pixel_group : pixels) {
    if (pixel_group.background.active) {
      const auto pos = pixel_group.background.screen_pos;
      background_framebuffer[SCREEN_WIDTH * pos.y + pos.x] =
          pixel_group.background.color;
    }
  }
    */
  }
  /*
  time += SDL_GetPerformanceCounter() - prev_time;
  printf("%lu\r", time / avg_count++);
  */

  auto sprites = render_sprites(scanline);
  for (const auto& sprite_pixels : sprites) {
    for (Pixel&& pixel : sprite_pixels) {
      if (pixel.color_index != 0 &&
          (pixel.bg_priority == BackgroundPosition::Above ||
           background_pixels[SCREEN_WIDTH * pixel.screen_pos.y +
                             pixel.screen_pos.x]
                   .color_index == 0)) {
        background_pixels[SCREEN_WIDTH * pixel.screen_pos.y +
                          pixel.screen_pos.x] = pixel;
      }
    }
  }
}

void Gpu::render() {
  std::transform(background_pixels.begin(), background_pixels.end(),
                 background_framebuffer.begin(),
                 [](const Pixel& pixel) { return pixel.color; });
  renderer->draw_pixels(background_texture, background_framebuffer);
  // renderer->present();
  std::fill(background_framebuffer.begin(), background_framebuffer.end(),
            Color{0, 0, 0, 0});
}
}  // namespace gb
