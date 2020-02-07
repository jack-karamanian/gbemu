#include "sdl_renderer.h"
#include <iostream>
#include "constants.h"

namespace gb {

static Uint32 MapRGB(const SDL_PixelFormat& format, Uint8 r, Uint8 g, Uint8 b) {
  return (r >> format.Rloss) << format.Rshift |
         (g >> format.Gloss) << format.Gshift |
         (b >> format.Bloss) << format.Bshift;
}

SdlRenderer::SdlRenderer(
    std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
        p_renderer)
    : renderer{std::move(p_renderer)},
      format{SDL_AllocFormat(SDL_PIXELFORMAT_RGB888), sdl::SdlDeleter()} {
  if (!format.get()) {
    std::cout << "SDL error :" << SDL_GetError() << std::endl;
  }
  create_texture(SCREEN_WIDTH, SCREEN_HEIGHT, false);
}

Texture SdlRenderer::create_texture(int width, int height, bool blend) {
  sdl::sdl_unique_ptr<SDL_Texture> new_texture{
      SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGB888,
                        SDL_TEXTUREACCESS_STREAMING, width, height),
      sdl::SdlDeleter()};

  if (blend) {
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
  }

  texture = std::move(new_texture);

  return {0};
}

void SdlRenderer::clear() {
  SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
  SDL_RenderClear(renderer.get());
}

void SdlRenderer::draw_pixels(const std::vector<Color>& pixels) {
  SDL_Texture* sdl_texture = this->texture.get();

  Uint32* texture_pixels = nullptr;
  int pitch = -1;

  if (SDL_LockTexture(sdl_texture, nullptr,
                      reinterpret_cast<void**>(&texture_pixels), &pitch)) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  const SDL_PixelFormat* pixel_format = format.get();
  for (int i = 0; i < DISPLAY_SIZE; ++i) {
    const Color& pixel = pixels[i];
    texture_pixels[i] = MapRGB(*pixel_format, pixel.r, pixel.g, pixel.b);
  }

  SDL_UnlockTexture(sdl_texture);

  SDL_RenderCopy(renderer.get(), sdl_texture, nullptr, nullptr);
}

void SdlRenderer::present() {
  SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr);
  SDL_RenderPresent(renderer.get());
}
}  // namespace gb
