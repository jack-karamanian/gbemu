#pragma once
#include "pixel.h"

namespace gb {
class IRenderer {
 public:
  virtual void draw_pixels(const Pixel* pixels) = 0;
};
}  // namespace gb
