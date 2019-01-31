#pragma once
#include <vector>
#include "constants.h"
#include "pixel.h"

namespace gb {
struct Texture {
  int handle = -1;
};
class IRenderer {
 public:
  virtual Texture create_texture(int width, int height, bool blend) = 0;
  virtual void clear() = 0;
  virtual void draw_pixels(Texture texture,
                           const std::vector<Pixel>& pixels) = 0;
  virtual void present() = 0;
  virtual ~IRenderer() noexcept {}
};
}  // namespace gb
