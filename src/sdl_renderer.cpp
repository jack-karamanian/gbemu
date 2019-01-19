#include <iostream>
#include "sdl_renderer.h"

namespace gb {
SdlRenderer::SdlRenderer(
    std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> renderer)
    : renderer{std::move(renderer)},
      texture{SDL_CreateTexture(this->renderer.get(),
                                SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                160,
                                144),
              [](auto t) { SDL_DestroyTexture(t); }} {
  if (!texture.get()) {
    std::cout << "SDL error :" << SDL_GetError() << std::endl;
  }
}

void SdlRenderer::draw_pixels(const Pixel* pixels) {
  Pixel* texture_pixels = nullptr;
  int pitch = -1;

  if (SDL_LockTexture(texture.get(), nullptr, (void**)&texture_pixels,
                      &pitch)) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  int size = 160 * 144;

  for (int y = 0; y < 144; ++y) {
    for (int x = 0; x < 160; ++x) {
      texture_pixels[160 * y + x] = pixels[144 * x + y];
    }
  }

  SDL_UnlockTexture(texture.get());

  SDL_RenderClear(renderer.get());

  SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);

  SDL_RenderPresent(renderer.get());
}
}  // namespace gb
