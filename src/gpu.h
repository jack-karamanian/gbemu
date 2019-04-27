#pragma once
#include <array>
#include <memory>
#include <vector>
#include "color.h"
#include "nonstd/span.hpp"
#include "registers/palette.h"
#include "renderer.h"
#include "types.h"
#include "utils.h"

namespace gb {
class Memory;
struct BgAttribute;

enum class PixelType {
  None,
  Sprite,
  Window,
  Background,
};

enum class BackgroundPosition { None, Above, Below };

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

class SdlRenderer;

class Gpu {
  Memory* memory;
  SdlRenderer* renderer;

  Texture background_texture;

  std::array<u8, SCREEN_WIDTH> background_color_indexes;
  std::vector<Color> background_framebuffer;

  std::array<u8, 64> cached_color_bytes;
  std::array<Color, 32> background_colors;

  std::array<std::array<Color, 4>, 2> sprite_colors;

  u8 render_pixel(const u8 byte1, const u8 byte2, const u8 pixel_x) const;
  void render_sprites(int scanline);
  void render_background(int scanline,
                         u16 tile_map_base,
                         nonstd::span<const u8> tile_map_range,
                         nonstd::span<const BgAttribute> tile_attribs,
                         u8 scx,
                         u8 scy,
                         int offset_x,
                         int offset_y);

  std::array<Color, 4> generate_colors(Palette palette, bool is_sprite = false);

  u8 scx = 0;
  u8 scy = 0;

  void render_background_pixels(int scanline,
                                std::pair<u16, u16> tile_map,
                                nonstd::span<const BgAttribute> tile_attribs,
                                u8 scx,
                                u8 scy,
                                int offset_x,
                                int offset_y);
  int color_palette_index = 0;

 public:
  Gpu(Memory& memory, SdlRenderer& renderer);

  void set_color_palette_index(int value) { color_palette_index = value; }

  u8 get_scx() const { return scx; }
  u8 get_scy() const { return scy; }

  void set_scx(u8 value) { scx = value; }
  void set_scy(u8 value) { scy = value; }

  void compute_background_palette(u8 palette);
  void compute_sprite_palette(int palette_number, u8 palette);

  u8 read_color_at_index() const;
  void compute_cgb_color(int index, u8 color);

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
