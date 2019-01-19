#pragma once
#include <SDL2/SDL.h>
#include <array>
#include <memory>
#include "pixel.h"
#include "types.h"

constexpr int DISPLAY_SIZE = 144 * 160;
namespace gb {
class IRenderer;
struct Memory;
class Gpu {
  std::array<Pixel, DISPLAY_SIZE> pixels = {{}};

  Memory* memory;
  std::unique_ptr<IRenderer> renderer;

 public:
  Gpu(Memory& memory, std::unique_ptr<IRenderer> renderer);

  void render();
};
}  // namespace gb
