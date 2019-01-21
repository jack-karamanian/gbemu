#pragma once
#include <SDL2/SDL.h>
#include <array>
#include <memory>
#include "constants.h"
#include "pixel.h"
#include "renderer.h"
#include "types.h"

namespace gb {
struct Memory;
class Gpu {
  Memory* memory;
  std::unique_ptr<IRenderer> renderer;

  std::array<Pixel, DISPLAY_SIZE> pixels = {{}};

  void dump_vram();
  u8 get_scx() const;
  u8 get_scy() const;
  void render_tile(const u8 byte1, const u8 byte2, const int x, const int y);
  void render_sprites(int scanline);
  void render_background(int scanline);

 public:
  Gpu(Memory& memory, std::unique_ptr<IRenderer> renderer);

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
