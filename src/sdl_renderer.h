#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <iostream>
#include <memory>
#include "constants.h"
#include "renderer.h"

namespace gb {
class SdlRenderer : public IRenderer {
  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> renderer;

  std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>> texture;

  std::unique_ptr<SDL_PixelFormat, std::function<void(SDL_PixelFormat*)>>
      format;

 public:
  SdlRenderer(std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
                  renderer);
  ~SdlRenderer() { std::cout << "SDL RENDERER DIED" << std::endl; }
  virtual void draw_pixels(
      const std::array<Pixel, DISPLAY_SIZE>& pixels) override;
};
}  // namespace gb
