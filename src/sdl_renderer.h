#pragma once
#include <SDL/SDL2.h>
#include <memory>
#include "renderer.h"

namespace gb {
class SdlRenderer : public Renderer {
  std::unique_ptr<SDL_Renderer> renderer;

 public:
  SdlRenderer(std::unique_ptr<SDL_Renderer> renderer);
  virtual void draw_pixels(const Pixel* pixels) override;
};
}  // namespace gb
