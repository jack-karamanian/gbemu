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
class SdlRenderer : public IRenderer {
  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> renderer;

  sdl::sdl_unique_ptr<SDL_PixelFormat> format;

  std::unordered_map<int, sdl::sdl_unique_ptr<SDL_Texture>> textures;
  std::vector<SDL_Texture*> draw_order;
  int draw_order_counter = 0;

 public:
  SdlRenderer(std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
                  renderer);
  virtual Texture create_texture(int width, int height, bool blend) override;
  virtual void clear() override;
  virtual void draw_pixels(Texture texture,
                           const std::vector<Color>& pixels) override;
  virtual void present() override;
};
}  // namespace gb
