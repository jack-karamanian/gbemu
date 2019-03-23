#include <iostream>
#include "sdl_renderer.h"

namespace gb {
SdlRenderer::SdlRenderer(
    std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
        p_renderer)
    : renderer{std::move(p_renderer)},
      format{SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), sdl::SdlDeleter()} {
  // SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);
  if (!format.get()) {
    std::cout << "SDL error :" << SDL_GetError() << std::endl;
  }
}

Texture SdlRenderer::create_texture(int width, int height, bool blend) {
  sdl::sdl_unique_ptr<SDL_Texture> texture{
      SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height),
      sdl::SdlDeleter()};

  if (blend) {
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
  }

  int id = textures.size();
  textures.emplace(id, std::move(texture));
  draw_order.resize(draw_order.size() + 1);

  return {id};
}

void SdlRenderer::clear() {
  SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
  SDL_RenderClear(renderer.get());
}

void SdlRenderer::draw_pixels(Texture texture,
                              const std::vector<Color>& pixels) {
  SDL_Texture* sdl_texture = textures.at(texture.handle).get();

  Uint32* texture_pixels = nullptr;
  int pitch = -1;

  if (SDL_LockTexture(sdl_texture, nullptr, (void**)&texture_pixels, &pitch)) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  nonstd::span<Uint32> texture_span(texture_pixels, DISPLAY_SIZE);

  SDL_PixelFormat* pixel_format = format.get();
  std::transform(pixels.begin(), pixels.end(), texture_span.begin(),
                 [pixel_format](Color pixel) {
                   return SDL_MapRGBA(pixel_format, pixel.r, pixel.g, pixel.b,
                                      pixel.a);
                 });

  SDL_UnlockTexture(sdl_texture);

  draw_order[draw_order_counter++] = sdl_texture;
  SDL_RenderCopy(renderer.get(), sdl_texture, nullptr, nullptr);
}

void SdlRenderer::present() {
  for (SDL_Texture* sdl_texture : draw_order) {
    SDL_RenderCopy(renderer.get(), sdl_texture, nullptr, nullptr);
  }
  SDL_RenderPresent(renderer.get());
  draw_order_counter = 0;
}
}  // namespace gb
