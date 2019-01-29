#include <iostream>
#include "sdl_renderer.h"

namespace gb {
SdlRenderer::SdlRenderer(
    std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> p_renderer)
    : renderer{std::move(p_renderer)},
      texture{SDL_CreateTexture(renderer.get(),
                                SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                160,
                                144),
              [](auto t) { SDL_DestroyTexture(t); }},
      format{SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), [](auto f) { SDL_FreeFormat(f); } } 
      {
  if (!texture.get() || !format.get()) {
    std::cout << "SDL error :" << SDL_GetError() << std::endl;
  }
}

void SdlRenderer::draw_pixels(const std::vector<Pixel>& pixels) {
  Uint32* texture_pixels = nullptr;
  int pitch = -1;

  if (SDL_LockTexture(texture.get(), nullptr, (void**)&texture_pixels,
                      &pitch)) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }


  for (int y = 0; y < 144; ++y) {
    for (int x = 0; x < 160; ++x) {
      const auto& [r, g, b, a] = pixels.at(144 * x + y);
      texture_pixels[160 * y + x] = SDL_MapRGBA(format.get(), r, g, b, a);
    }
  }

  SDL_UnlockTexture(texture.get());

  SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
  SDL_RenderClear(renderer.get());

  SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);

  SDL_RenderPresent(renderer.get());
}
}  // namespace gb
