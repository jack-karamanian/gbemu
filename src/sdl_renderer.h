#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <memory>
#include "renderer.h"

namespace gb {
class SdlRenderer : public IRenderer {
  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> renderer;

  std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>> texture;

 public:
  SdlRenderer(std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
                  renderer);
  virtual void draw_pixels(const Pixel* pixels) override;
};
}  // namespace gb
