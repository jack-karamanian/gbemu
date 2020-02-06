#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <iostream>
#include <memory>
#include <nonstd/span.hpp>
#include <unordered_map>
#include <vector>
#include "constants.h"
#include "renderer.h"
#include "sdl_utils.h"

namespace gb {
class SdlRenderer {
  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> renderer;

  sdl::sdl_unique_ptr<SDL_PixelFormat> format;

  sdl::sdl_unique_ptr<SDL_Texture> texture;

 public:
  SdlRenderer(std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
                  renderer);
  Texture create_texture(int width, int height, bool blend);
  void clear();
  void draw_pixels(const std::vector<Color>& pixels);
  void present();
};
}  // namespace gb
