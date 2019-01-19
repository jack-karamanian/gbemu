#pragma once
#include <SDL2/SDL.h>
#include <memory>
#include "pixel.h"
#include "system.h"
#include "types.h"

#define DISPLAY_SIZE 144 * 160
namespace gb {
struct Gpu {
  const System* const system;
  Gpu(const System* const system, SDL_Renderer* const renderer);
  ~Gpu();

  void render();
  Pixel pixels[DISPLAY_SIZE];

  SDL_Renderer* const renderer;
  SDL_Texture* screen_texture;
};
}  // namespace gb
