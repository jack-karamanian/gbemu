#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/combine.hpp>
#include <boost/range/irange.hpp>
#include "constants.h"
#include "gpu.h"
#include "memory.h"
#include "registers/lcdc.h"
#include "sprite_attribute.h"

static constexpr u16 VRAM_START = 0x8000;

namespace gb {
static constexpr std::array<Color, 4> COLORS = {{
    {255, 255, 255},
    {128, 128, 128},
    {100, 100, 100},
    {32, 32, 32},
}};

static constexpr std::array<Color, 4> SPRITE_COLORS = {{
    {255, 255, 255},
    {128, 128, 128},
    {100, 100, 100},
    {0, 0, 0},
}};

Gpu::Gpu(Memory& memory, std::shared_ptr<IRenderer> renderer)
    : memory{&memory},
      renderer{std::move(renderer)},
      background_texture{
          this->renderer->create_texture(SCREEN_WIDTH, SCREEN_HEIGHT, false)},
      background_framebuffer(DISPLAY_SIZE),
      background_colors(COLORS),
      sprite_colors{SPRITE_COLORS, SPRITE_COLORS},
      background_pixels(SCREEN_WIDTH) {}

u8 Gpu::get_scx() const {
  return memory->get_ram(0xff43);
}

u8 Gpu::get_scy() const {
  return memory->get_ram(0xff42);
}

u8 Gpu::render_pixel(const u8 byte1, const u8 byte2, const u8 pixel_x) const {
  const u8 shift = 7 - pixel_x;
  const u8 mask = (0x1 << shift);
  const u8 low_bit = (byte1 & mask) >> shift;
  const u8 high_bit = (byte2 & mask) >> shift;

  const u8 color_index = (high_bit << 1) | low_bit;

  return color_index;
}

auto Gpu::render_sprites(int scanline) const {
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
      sprite_attribs | boost::adaptors::filtered([](auto sprite_attrib) {
        return sprite_attrib.x > 0 && sprite_attrib.x < 168 &&
               sprite_attrib.y > 0 && sprite_attrib.y < 160;
      }) |
      boost::adaptors::filtered([scanline, sprite_height](auto sprite_attrib) {
        const int adjusted_y = sprite_attrib.y - 16;
        return scanline >= adjusted_y && scanline < adjusted_y + sprite_height;
      }) |
      boost::adaptors::transformed([this, scanline,
                                    sprite_height](auto sprite_attrib) {
        const int adjusted_y = sprite_attrib.y - 16;

        const int sprite_palette = sprite_attrib.palette_number();

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

        return boost::irange(0, 8) |
               boost::adaptors::transformed([this, sprite_attrib, x, y, byte1,
                                             byte2,
                                             sprite_palette](int pixel_x) {
                 const int flipped_pixel_x =
                     sprite_attrib.flip_x() ? 7 - pixel_x : pixel_x;
                 const int screen_x = x + pixel_x;
                 const u8 color_index =
                     render_pixel(byte1, byte2, flipped_pixel_x);
                 return Pixel{{static_cast<u8>(screen_x), static_cast<u8>(y)},
                              color_index,
                              static_cast<s8>(sprite_palette),
                              sprite_attrib.above_bg()};
               }) |
               boost::adaptors::filtered([](auto&& pixel) {
                 return pixel.screen_pos.x >= 0 && pixel.screen_pos.x < 160;
               });
      });

  return sprite_pixels;
}

auto Gpu::render_background(
    int scanline,
    nonstd::span<const u8> tile_map_range,
    u8 scx,
    u8 scy,
    int offset_x,  // TODO: figure out what to do with this
    int offset_y) const {
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};

  const bool is_signed = lcdc.is_tile_map_signed();

  const int scanline_tile_row = (scanline + scy + offset_y) / 8;
  const u16 tile_scroll_offset = 32 * scanline_tile_row;

  const int tile_y = ((scanline + scy) % 8);

  const int screen_y = scanline + offset_y;

  const auto tile_data = memory->get_range(lcdc.bg_tile_data_range());

  const int tile_map_range_size = tile_map_range.size();

  auto addrs =
      boost::irange(0, 160) | boost::adaptors::transformed([=](u16 x) {
        const u16 tile_index = tile_scroll_offset + ((x + scx) / 8) % 32;

        const u8 tile_num =
            tile_map_range[tile_index > tile_map_range_size - 1
                               ? tile_index - tile_map_range_size
                               : tile_index];

        const u16 tile_addr =
            (16 * (is_signed
                       ? static_cast<int16_t>(static_cast<s8>(tile_num)) + 128
                       : tile_num)) +
            2 * tile_y;

        const u8 byte1 = tile_data[tile_addr];
        const u8 byte2 = tile_data[tile_addr + 1];

        const u8 color_index = render_pixel(byte1, byte2, (x + scx) % 8);
        return Pixel{Vec2<u8>{static_cast<u8>(x), static_cast<u8>(screen_y)},
                     color_index, false};
      });
  return addrs;
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

void Gpu::render_scanline(int scanline) {
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};
  const u16 window_y = memory->get_ram(0xff4a);

  if (lcdc.window_on() && scanline >= window_y) {
    // const u16 window_x = memory->get_ram(0xff4b) - 7;
    const auto tile_map_range = memory->get_range(lcdc.window_tile_map_range());
    auto window =
        render_background(scanline, tile_map_range, 0, 0, 0, -window_y);
    std::copy(window.begin(), window.end(), background_pixels.begin());
  } else if (lcdc.bg_on()) {
    const auto tile_map_range = memory->get_range(lcdc.bg_tile_map_range());

    auto background =
        render_background(scanline, tile_map_range, get_scx(), get_scy(), 0, 0);

    std::copy(background.begin(), background.end(), background_pixels.begin());
  }

  auto sprites = render_sprites(scanline);
  for (const auto& sprite_pixels : sprites) {
    for (Pixel&& pixel : sprite_pixels) {
      if (pixel.color_index != 0 &&
          (pixel.above_bg ||
           background_pixels[pixel.screen_pos.x].color_index == 0)) {
        background_pixels.at(pixel.screen_pos.x) = pixel;
      }
    }
  }

  std::transform(
      background_pixels.begin(), background_pixels.end(),
      background_framebuffer.begin() + SCREEN_WIDTH * scanline,
      [this](const Pixel& pixel) {
        if (pixel.sprite_palette > -1) {
          return sprite_colors.at(pixel.sprite_palette).at(pixel.color_index);
        }
        return background_colors.at(pixel.color_index);
      });
}

void Gpu::render() {
  renderer->draw_pixels(background_texture, background_framebuffer);
}
}  // namespace gb
