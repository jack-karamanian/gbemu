#include <iostream>
#include "sdl_renderer.h"

namespace gb {
SdlRenderer::SdlRenderer(
    std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>> p_renderer)
    : renderer{std::move(p_renderer)},
      format{SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), sdl::SdlDeleter() } 
      {
        //SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);
  if (!format.get()) {
    std::cout << "SDL error :" << SDL_GetError() << std::endl;
  }
}

Texture SdlRenderer::create_texture(int width, int height, bool blend) {
  sdl::sdl_unique_ptr<SDL_Texture> texture{
    SDL_CreateTexture(renderer.get(),
                                SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                width,
                                height),
      sdl::SdlDeleter()
  };

  if (blend) {
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
  }

  int id = textures.size();
  textures.emplace(id, std::move(texture));

  return {id};
}

void SdlRenderer::clear() {
  SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
  SDL_RenderClear(renderer.get());
}

void SdlRenderer::draw_pixels(Texture texture, const std::vector<Pixel>& pixels) {
  SDL_Texture* sdl_texture = textures.at(texture.handle).get();

  Uint32* texture_pixels = nullptr;
  int pitch = -1;

  if (SDL_LockTexture(sdl_texture, nullptr, (void**)&texture_pixels,
                      &pitch)) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }


  for (int y = 0; y < 144; ++y) {
    for (int x = 0; x < 160; ++x) {
      const auto& [r, g, b, a] = pixels.at(144 * x + y);
      texture_pixels[160 * y + x] = SDL_MapRGBA(format.get(), r, g, b, a);
    }
  }

  SDL_UnlockTexture(sdl_texture);
  SDL_RenderCopy(renderer.get(), sdl_texture, nullptr, nullptr);
}

void SdlRenderer::present() {
  SDL_RenderPresent(renderer.get());
}
}  // namespace gb
