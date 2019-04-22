#include <boost/range/irange.hpp>
#include "constants.h"
#include "gpu.h"
#include "memory.h"
#include "registers/lcdc.h"
#include "sdl_renderer.h"
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

Gpu::Gpu(Memory& memory, SdlRenderer& sdl_renderer)
    : memory{&memory},
      renderer{&sdl_renderer},
      background_texture{
          renderer->create_texture(SCREEN_WIDTH, SCREEN_HEIGHT, false)},
      background_framebuffer(DISPLAY_SIZE),
      background_colors{COLORS[0], COLORS[1], COLORS[2], COLORS[3]},
      sprite_colors{SPRITE_COLORS, SPRITE_COLORS} {}

u8 Gpu::render_pixel(const u8 byte1, const u8 byte2, const u8 pixel_x) const {
  const u8 shift = 7 - pixel_x;
  const u8 mask = (0x1 << shift);
  const u8 low_bit = (byte1 & mask) >> shift;
  const u8 high_bit = (byte2 & mask) >> shift;

  const u8 color_index = (high_bit << 1) | low_bit;

  return color_index;
}

void Gpu::render_sprites(int scanline) {
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};
  auto sprite_attribs = memory->get_sprite_attributes();

  const int sprite_height = [lcdc] {
    switch (lcdc.sprite_size()) {
      case Registers::Lcdc::SpriteMode::EightByEight:
        return 8;
      case Registers::Lcdc::SpriteMode::EightBySixteen:
        return 16;
    }
  }();

  for (const auto& sprite_attrib : sprite_attribs) {
    const int adjusted_y = sprite_attrib.y - 16;
    if (sprite_attrib.x > 0 && sprite_attrib.x < 168 && sprite_attrib.y > 0 &&
        sprite_attrib.y < 160 && scanline >= adjusted_y &&
        scanline < adjusted_y + sprite_height) {
      const int sprite_palette = sprite_attrib.palette_number();

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
      const auto& selected_colors = sprite_colors[sprite_palette];

      for (int pixel_x :
           boost::irange(x < 0 ? -x : 0,
                         x + TILE_SIZE >= SCREEN_WIDTH
                             ? TILE_SIZE - ((x + TILE_SIZE) - SCREEN_WIDTH)
                             : TILE_SIZE)) {
        const int flipped_pixel_x =
            sprite_attrib.flip_x() ? 7 - pixel_x : pixel_x;
        const int screen_x = x + pixel_x;

        // std::cout << x << ' ' << pixel_x << ' ' << screen_x << '\n';
        assert(screen_x >= 0 && screen_x < SCREEN_WIDTH);

        const u8 color_index = render_pixel(byte1, byte2, flipped_pixel_x);
        const u8 bg_color = background_color_indexes[screen_x] % 4;

        if (color_index != 0 && (sprite_attrib.above_bg() || bg_color == 0)) {
          background_framebuffer[SCREEN_WIDTH * scanline + screen_x] =
              selected_colors[color_index];
        }
      }
    }
  }
}

void Gpu::render_background(
    int scanline,
    u16 tile_map_base,
    nonstd::span<const u8> tile_map_range,
    nonstd::span<const BgAttribute> tile_attribs,
    u8 scroll_x,
    u8 scroll_y,
    int offset_x,  // TODO: figure out what to do with this
    int offset_y) {
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};

  const bool is_signed = lcdc.is_tile_map_signed();

  const u16 y_base = scanline + scroll_y + offset_y;

  const int tile_y = ((scanline + scroll_y) % 8);

  const int screen_y = scanline + offset_y;

  const auto tile_data_range = lcdc.bg_tile_data_range();
  const auto tile_data = memory->get_range(tile_data_range);

  const int tile_map_range_size = tile_map_range.size();

  const u8 table_selected = tile_map_base == 0x9800 ? 0 : 1;

  // From https://gist.github.com/drhelius/3730564
  const u16 tile_base = 0x9800 | (table_selected << 10) |
                        ((y_base & 0xf8) << 2) | ((scroll_x & 0xf8) >> 3);

  // HACK: The real hardware renders 20-22 tiles depending on scroll_x
  // but let's just always render 21 tiles
  for (int tile : boost::irange(0, 21)) {
    // Add which tile to be displayed to the last 5 bits of tile_base
    const u16 tile_num_addr =
        (tile_base & ~0x1f) | (((tile_base & 0x1f) + tile) & 0x1f);

    const u16 tile_index = tile_num_addr - tile_map_base;

    // Convert from [0, 21) to [0, 160]
    const u16 tile_x = tile * TILE_SIZE;
    const u16 adjusted_tile_index = tile_index > tile_map_range_size - 1
                                        ? tile_index - tile_map_range_size
                                        : tile_index;

    assert(adjusted_tile_index < tile_map_range.size());
    const u8 tile_num = tile_map_range[adjusted_tile_index];

    const auto tile_attrib = tile_attribs[adjusted_tile_index];

    // Calculate the address of the tile pixels, flipping vertically if
    // specified
    const u16 tile_addr =
        (16 * (is_signed ? static_cast<int16_t>(static_cast<s8>(tile_num)) + 128
                         : tile_num)) +
        2 * (tile_attrib.vertical_flip() ? TILE_SIZE - tile_y - 1 : tile_y);

    const auto tile_vram =
        tile_attrib.vram_bank() == 1
            ? nonstd::span<const u8>{&memory->get_vram_bank1()[0] +
                                         (tile_data_range.first - 0x8000),
                                     4096}
            : tile_data;

    assert(tile_addr < tile_vram.size());
    assert(tile_addr + 1 < tile_vram.size());
    const u8 byte1 = tile_vram[tile_addr];
    const u8 byte2 = tile_vram[tile_addr + 1];

    const u16 tile_scroll_x = scroll_x % TILE_SIZE;

    const int x_begin = static_cast<int>(
        tile_x - tile_scroll_x < 0
            ? tile_scroll_x
            : 0);  // Make sure x does not underflow when scroll_x > 0
    const int x_end = tile_x >= SCREEN_WIDTH
                          ? tile_scroll_x
                          : TILE_SIZE;  // Make sure x is < 160

    for (int pixel_x : boost::irange(x_begin, x_end)) {
      const u16 x = tile_x + pixel_x - tile_scroll_x;

      assert(x >= 0 && x < 160);

      const u8 color_index = render_pixel(
          byte1, byte2,
          tile_attrib.horizontal_flip() ? TILE_SIZE - pixel_x - 1 : pixel_x);

      const u8 palette_color_index =
          static_cast<u8>((4 * tile_attrib.bg_palette()) + color_index);
      background_framebuffer[160 * scanline + x] =
          background_colors[palette_color_index];
      background_color_indexes[x] = palette_color_index;
    }
  }
}

std::array<Color, 4> Gpu::generate_colors(Palette palette, bool is_sprite) {
  const auto& base_colors = is_sprite ? SPRITE_COLORS : COLORS;
  std::array<Color, 4> colors;
  std::generate(colors.begin(), colors.end(),
                [palette, base_colors, i = 0]() mutable {
                  return base_colors.at(palette.get_color(i++));
                });
  return colors;
}

void Gpu::render_background_pixels(int scanline,
                                   std::pair<u16, u16> tile_map,
                                   nonstd::span<const BgAttribute> tile_attribs,
                                   u8 scroll_x,
                                   u8 scroll_y,
                                   int offset_x,
                                   int offset_y) {
  const auto tile_map_range = memory->get_range(tile_map);
  render_background(scanline, tile_map.first, tile_map_range, tile_attribs,
                    scroll_x, scroll_y, offset_x, offset_y);
}

void Gpu::compute_background_palette(u8 palette) {
  auto colors = generate_colors({palette});
  std::copy(colors.begin(), colors.end(), background_colors.begin());
}

void Gpu::compute_sprite_palette(int palette_number, u8 palette) {
  sprite_colors.at(palette_number) = generate_colors({palette}, true);
}

void Gpu::compute_cgb_color(int index, u8 color) {
  const int color_index = index / 2;
  Color& background_color = background_colors[color_index];

  const CgbColor cgb_color{index, color};
  if (index % 2 == 0) {
    background_color.r = (cgb_color.r() * 255) / 32;
    background_color.g = (cgb_color.g() * 255) / 32;
  } else {
    const u8 green = (background_color.g * 32) / 255;
    background_color.g = ((cgb_color.g() | green) * 255) / 32;
    background_color.b = (cgb_color.b() * 255) / 32;
  }
}

void Gpu::render_scanline(int scanline) {
  const Registers::Lcdc lcdc{memory->get_ram(Registers::Lcdc::Address)};
  const u16 window_y = memory->get_ram(0xff4a);

  if (lcdc.window_on() && scanline >= window_y) {
    const auto range = lcdc.window_tile_map_range();
    const auto tile_attribs = memory->get_window_attributes();
    render_background_pixels(scanline, range, tile_attribs, 0, 0, 0, -window_y);
  } else if (lcdc.bg_on()) {
    const auto range = lcdc.bg_tile_map_range();
    const auto tile_attribs = memory->get_background_attributes();
    render_background_pixels(scanline, range, tile_attribs, scx, scy, 0, 0);
  }

  render_sprites(scanline);
}

void Gpu::render() {
  renderer->draw_pixels(background_texture, background_framebuffer);
}
}  // namespace gb
