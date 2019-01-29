#pragma once
#include <vector>
#include "constants.h"
#include "pixel.h"

namespace gb {
class IRenderer {
 public:
  virtual void draw_pixels(const std::vector<Pixel>& pixels) = 0;
  virtual ~IRenderer() noexcept {}
};
}  // namespace gb
