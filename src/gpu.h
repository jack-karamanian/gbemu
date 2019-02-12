#pragma once
#include <SDL2/SDL.h>
#include <memory>
#include <vector>
#include "pixel.h"
#include "renderer.h"
#include "types.h"

namespace gb {
struct Memory;
class Gpu {
  Memory* memory;
  std::shared_ptr<IRenderer> renderer;

  Texture background_texture;
  Texture sprite_texture;

  std::vector<Pixel> background_framebuffer;
  std::vector<Pixel> sprite_framebuffer;

  u8 get_scx() const;
  u8 get_scy() const;
  void render_pixel(std::vector<Pixel>& pixels,
                    const u8 byte1,
                    const u8 byte2,
                    const u8 pixel_x,
                    const int x,
                    const int y,
                    const std::array<Pixel, 4>& colors);
  void render_sprites(int scanline);
  void render_background(int scanline);

 public:
  Gpu(Memory& memory, std::shared_ptr<IRenderer> renderer);

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
