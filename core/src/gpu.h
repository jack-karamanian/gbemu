#pragma once
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "color.h"
#include "nonstd/span.hpp"
#include "registers/palette.h"
#include "renderer.h"
#include "sprite_attribute.h"
#include "types.h"
#include "utils.h"

namespace gb {
class Memory;
struct BgAttribute;

class CgbColor {
 public:
  CgbColor(int index, u8 color)
      : value{static_cast<u16>(index % 2 == 0 ? color : color << 8)} {}

  [[nodiscard]] u8 r() const { return value & 0x001f; }
  [[nodiscard]] u8 g() const { return (value & 0x03e0) >> 5; }
  [[nodiscard]] u8 b() const { return (value & 0x7c00) >> 10; }

 private:
  u16 value;
};

struct CgbPaletteIndex {
  u8 value = 0;

  [[nodiscard]] u8 index() const { return value & 0x3f; }

  [[nodiscard]] bool auto_increment() const { return test_bit(value, 7); }

  void increment_index() { value = increment_bits(value, 0x3f); }
};

struct CgbPalette {
  std::vector<u8> color_bytes;
  std::vector<Color> colors;
  CgbPaletteIndex index;

  CgbPalette() : color_bytes(64, 0), colors(32) {}

  [[nodiscard]] nonstd::span<const Color> colors_for_palette(
      int palette) const {
    return {&colors[4 * palette], 4};
  }

  [[nodiscard]] u8 current_color_byte() const {
    return color_bytes[index.index()];
  }
};

struct BgPixel {
  bool priority : 1;
  u8 color_index : 7;
};

class SdlRenderer;

class Gpu {
  using SpriteFilter = std::function<SpriteAttribute(SpriteAttribute)>;
  Memory* memory;
  SdlRenderer* renderer;

  SpriteFilter sprite_filter;

  CgbPalette background_palette;
  CgbPalette sprite_palette;

  std::array<BgPixel, SCREEN_WIDTH> background_pixels;
  std::vector<Color> background_framebuffer;

  [[nodiscard]] u8 render_pixel(u8 byte1, u8 byte2, u8 pixel_x) const;
  void render_sprites(int scanline);
  void render_background(int scanline,
                         u16 tile_map_base,
                         nonstd::span<const u8> tile_map_range,
                         nonstd::span<const u8> tile_attribs,
                         u8 scx,
                         u8 scy,
                         int offset_x,
                         int offset_y);

  std::array<Color, 4> generate_colors(Palette palette, bool is_sprite = false);

  void render_background_pixels(int scanline,
                                std::pair<u16, u16> tile_map,
                                nonstd::span<const u8> tile_attribs,
                                u8 scx,
                                u8 scy,
                                int offset_x,
                                int offset_y);

  Color compute_cgb_color(Color real_color, int index, u8 color);
  void add_color_to_palette(CgbPalette& palette, u8 color);

 public:
  u8 scx = 0;
  u8 scy = 0;

  u8 window_y = 0;
  u8 window_x = 0;

  Gpu(Memory& memory, SdlRenderer& renderer, SpriteFilter filter);

  void compute_background_palette(u8 palette);
  void compute_sprite_palette(int palette_number, u8 palette);

  [[nodiscard]] u8 read_background_color() const;

  [[nodiscard]] u8 read_sprite_color() const;

  void set_background_color_index(u8 value) {
    background_palette.index.value = value;
  }

  [[nodiscard]] u8 background_palette_index() const {
    return background_palette.index.index();
  }

  void set_sprite_color_index(u8 value) { sprite_palette.index.value = value; }

  [[nodiscard]] u8 sprite_palette_index() const {
    return sprite_palette.index.index();
  }

  void compute_background_color(u8 color);

  void compute_sprite_color(u8 color);

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
