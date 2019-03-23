#pragma once
#include <SDL2/SDL.h>
#include <array>
#include <memory>
#include <vector>
#include "nonstd/span.hpp"
#include "pixel.h"
#include "registers/palette.h"
#include "renderer.h"
#include "utils.h"

namespace gb {
class Memory;

enum class PixelType {
  None,
  Sprite,
  Window,
  Background,
};

enum class BackgroundPosition { None, Above, Below };

struct Pixel {
  BackgroundPosition bg_priority;

  Vec2<int> screen_pos;

  Color color;

  int color_index;

  Pixel() {}

  Pixel(BackgroundPosition bg_priority,
        Vec2<int> screen_pos,
        Color color,
        int color_index)
      : bg_priority{bg_priority},
        screen_pos{screen_pos},
        color{color},
        color_index{color_index} {}
};

class Gpu {
  Memory* memory;
  std::shared_ptr<IRenderer> renderer;

  Texture background_texture;

  std::vector<Color> background_framebuffer;

  std::array<Color, 4> background_colors;
  std::array<std::array<Color, 4>, 2> sprite_colors;

  std::vector<Pixel> background_pixels;
  std::array<u8, 160> color_index_cache;

  u8 get_scx() const;
  u8 get_scy() const;

  std::pair<Color, u8> render_pixel(const u8 byte1,
                                    const u8 byte2,
                                    const u8 pixel_x,
                                    const std::array<Color, 4>& colors) const;
  auto render_sprites(int scanline) const;
  auto render_background(int scanline,
                         nonstd::span<const u8> tile_map_range,
                         u8 scx,
                         u8 scy,
                         int offset_x,
                         int offset_y);
  void reconcile_framebuffer();

  void render_tile_map(nonstd::span<const u8> tile_map);

  std::array<Color, 4> generate_colors(Palette palette, bool is_sprite = false);

 public:
  Gpu(Memory& memory, std::shared_ptr<IRenderer> renderer);

  void compute_background_palette(u8 palette);
  void compute_sprite_palette(int palette_number, u8 palette);

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
