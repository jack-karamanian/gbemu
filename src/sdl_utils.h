#pragma once
#include <SDL2/SDL.h>
#include <memory>

namespace sdl {
struct SdlDeleter {
  void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); }
  void operator()(SDL_PixelFormat* format) const { SDL_FreeFormat(format); }
};

template <typename T>
using sdl_unique_ptr = std::unique_ptr<T, SdlDeleter>;
}  // namespace sdl
