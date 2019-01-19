#include "sdl_renderer.h"

namespace gb {
SdlRenderer::SdlRenderer(std::unique_ptr<SDL_Renderer> renderer)
    : renderer{std::move(renderer)} {}

void SdlRenderer::draw_pixels(const Pixel* pixels) {}
}  // namespace gb
