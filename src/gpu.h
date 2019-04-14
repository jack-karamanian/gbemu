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

struct Pixel {
  Vec2<u8> screen_pos = {0, 0};

  u8 color_index = 0;
  s8 sprite_palette = -1;

  bool above_bg = false;

  Pixel() {}

  Pixel(Vec2<u8> screen_pos, u8 color_index, bool above_bg)
      : screen_pos{screen_pos}, color_index{color_index}, above_bg{above_bg} {}

  Pixel(Vec2<u8> screen_pos, u8 color_index, s8 sprite_palette, bool above_bg)
      : screen_pos{screen_pos},
        color_index{color_index},
        sprite_palette{sprite_palette},
        above_bg{above_bg} {}
};

class CgbColor {
 public:
  CgbColor(int index, u8 color)
      : value{static_cast<u16>(index % 2 == 0 ? color : color << 8)} {}

  u8 r() const { return value & 0x001f; }
  u8 g() const { return (value & 0x03e0) >> 5; }
  u8 b() const { return (value & 0x7c00) >> 10; }

 private:
  u16 value;
};

class Gpu {
  Memory* memory;
  std::shared_ptr<IRenderer> renderer;

  Texture background_texture;

  std::vector<Color> background_framebuffer;

  std::array<Color, 32> background_colors;
  std::array<std::array<Color, 4>, 2> sprite_colors;

  std::vector<Pixel> background_pixels;

  u8 get_scx() const;
  u8 get_scy() const;

  u8 render_pixel(const u8 byte1, const u8 byte2, const u8 pixel_x) const;
  auto render_sprites(int scanline) const;
  auto render_background(int scanline,
                         u16 tile_map_base,
                         nonstd::span<const u8> tile_map_range,
                         nonstd::span<const BgAttribute> tile_attribs,
                         u8 scx,
                         u8 scy,
                         int offset_x,
                         int offset_y) const;

  std::array<Color, 4> generate_colors(Palette palette, bool is_sprite = false);

  void render_background_pixels(int scanline,
                                std::pair<u16, u16> tile_map,
                                nonstd::span<const BgAttribute> tile_attribs,
                                u8 scx,
                                u8 scy,
                                int offset_x,
                                int offset_y);

 public:
  Gpu(Memory& memory, std::shared_ptr<IRenderer> renderer);

  void compute_background_palette(u8 palette);
  void compute_sprite_palette(int palette_number, u8 palette);

  void compute_cgb_color(int index, u8 color);

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
