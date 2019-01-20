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
  void render_sprites(int scanline);

 public:
  Gpu(Memory& memory, std::unique_ptr<IRenderer> renderer);
  ~Gpu();

  void render();
  void render_scanline(int scanline);
};
}  // namespace gb
