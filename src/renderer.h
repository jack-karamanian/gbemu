#pragma once
#include <array>
#include "constants.h"
#include "pixel.h"

namespace gb {
class IRenderer {
 public:
  virtual void draw_pixels(const std::array<Pixel, DISPLAY_SIZE>& pixels) = 0;
  virtual ~IRenderer() noexcept {}
};
}  // namespace gb
