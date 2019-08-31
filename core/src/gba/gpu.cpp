#include "gba/gpu.h"
#include <chrono>
#include <range/v3/all.hpp>
#include "gba/mmu.h"

namespace gb::advance {

void Dispcnt::on_after_write() const {
  m_gpu->sort_backgrounds();
}

void Bgcnt::on_after_write() const {
  m_gpu->sort_backgrounds();
}

class TileMapEntry : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] int tile_id() const { return m_value & 0b11'1111'1111; }
  [[nodiscard]] int palette_bank() const { return (m_value >> 12) & 0xf; }
};

class ObjAttribute0 : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] int y() const { return m_value & 0xff; }

  enum class Mode {
    Normal,
    Affine,
    Disable,
    AffineDoubleRendering,
  };

  [[nodiscard]] Mode mode() const {
    return static_cast<Mode>((m_value >> 8) & 0b11);
  }

  enum class GfxMode {
    Normal,
    AlphaBlending,
    Window,
    Forbidden,
  };

  [[nodiscard]] GfxMode gfx_mode() const {
    return static_cast<GfxMode>((m_value >> 10) & 0b11);
  }

  [[nodiscard]] bool mosaic() const { return test_bit(12); }

  [[nodiscard]] int bits_per_pixel() const { return test_bit(13) ? 8 : 4; }

  enum class Shape {
    Square = 0,
    Horizontal = 1,
    Vertical = 2,
  };

  [[nodiscard]] Shape shape() const {
    return static_cast<Shape>((m_value >> 14) & 0b11);
  }
};

class ObjAttribute1 : public Integer<u16> {
 public:
  using Integer::Integer;
  [[nodiscard]] int x() const { return m_value & 0b1'1111'1111; }

  [[nodiscard]] bool horizontal_flip() const { return test_bit(12); }

  [[nodiscard]] bool vertical_flip() const { return test_bit(13); }

  [[nodiscard]] int obj_size() const { return (m_value >> 14) & 0b11; }
};

class ObjAttribute2 : public Integer<u16> {
 public:
  using Integer::Integer;
  [[nodiscard]] int tile_id() const { return m_value & 0b11'1111'1111; }

  [[nodiscard]] int priority() const { return (m_value >> 10) & 0b11; }

  [[nodiscard]] int palette_bank() const { return (m_value >> 12) & 0b1111; }
};

struct Sprite {
  ObjAttribute0 attrib0;
  ObjAttribute1 attrib1;
  ObjAttribute2 attrib2;

  Sprite(u16 attrib0_value, u16 attrib1_value, u16 attrib2_value)
      : attrib0{attrib0_value},
        attrib1{attrib1_value},
        attrib2{attrib2_value} {}
};

static Rect<unsigned int> sprite_size(ObjAttribute0::Shape shape,
                                      int size_index) {
  static constexpr std::array<Rect<unsigned int>, 12> sprite_sizes = {{
      {8, 8},    // 0, Square (Size, Shape)
      {16, 8},   // 0, Horizontal
      {8, 16},   // 0, Vertical
      {16, 16},  // 1, Square
      {32, 8},   // 1, Horizontal
      {8, 32},   // 1, Vertical
      {32, 32},  // 2, Square
      {32, 16},  // 2, Horizontal
      {16, 32},  // 2, Vertical
      {64, 64},  // 3, Square
      {64, 32},  // 3, Horizontal
      {32, 64},  // 3, Vertical
  }};

  const int shape_index = static_cast<int>(shape);

  return sprite_sizes[3 * size_index + shape_index];
}

void Gpu::render_scanline(int scanline) {
  std::fill(m_framebuffer.begin() + ScreenWidth * scanline,
            m_framebuffer.begin() + ScreenWidth * scanline + ScreenWidth,
            Color{0, 0, 0, 255});
  for (auto background = m_backgrounds.begin(); background != m_backgrounds_end;
       ++background) {
    render_background(**background, scanline);
  }
  render_sprites(scanline);
}

void Gpu::sort_backgrounds() {
  auto i = std::partition(m_backgrounds.begin(), m_backgrounds.end(),
                          [this](auto background) {
                            return dispcnt.layer_enabled(background->layer);
                          });
  constexpr_sort(m_backgrounds.begin(), i, [](auto* a, auto* b) {
    return a->control.priority() > b->control.priority();
  });
  m_backgrounds_end = i;
}

static void draw_color(nonstd::span<Color> framebuffer,
                       unsigned int x,
                       unsigned int y,
                       u16 color) {
  framebuffer[Gpu::ScreenWidth * y + x] = {
      static_cast<u8>(convert_space<32, 255>(color & 0x1f)),
      static_cast<u8>(convert_space<32, 255>((color >> 5) & 0x1f)),
      static_cast<u8>(convert_space<32, 255>((color >> 10) & 0x1f)), 255};
}

static void render_tile_row_4bpp(nonstd::span<Color> framebuffer,
                                 nonstd::span<const u8> palette_bank,
                                 Vec2<unsigned int> position,
                                 nonstd::span<const u8> tile_pixels) {
  const auto [base_index, scanline] = position;
  for (const auto [pixel_x, tile_group] :
       ranges::view::enumerate(tile_pixels)) {
    const u8 pixel_one = tile_group & 0xf;
    const u8 pixel_two = (tile_group >> 4) & 0xf;

    const u16 color_one = pixel_one == 0
                              ? 0
                              : (palette_bank[pixel_one * 2] |
                                 (palette_bank[pixel_one * 2 + 1] << 8));
    const u16 color_two = pixel_two == 0
                              ? 0
                              : (palette_bank[pixel_two * 2] |
                                 (palette_bank[pixel_two * 2 + 1] << 8));

    if (pixel_one != 0) {
      draw_color(framebuffer, (base_index + pixel_x * 2) % Gpu::ScreenWidth,
                 scanline, color_one);
    }
    if (pixel_two != 0) {
      draw_color(framebuffer, (base_index + 1 + pixel_x * 2) % Gpu::ScreenWidth,
                 scanline, color_two);
    }
  }
}

void Gpu::render_background(Background background, int scanline) {
  const u32 tile_scanline = scanline / TileSize;
  const u16 tile_x = background.scroll.x / TileSize;
  const u16 tile_y = background.scroll.y / TileSize;

  const Bgcnt control = background.control;

  const auto screen_size = control.screen_size();
  const auto tile_screen_size = screen_size.as_tiles();

  const auto base_block_offset =
      control.tilemap_base_block() +
      (tile_screen_size.screen_size.width * tile_y + tile_x +
       (tile_screen_size.screen_size.width * 2 * tile_scanline));
  const nonstd::span<const u8> vram = m_vram.subspan(base_block_offset);

  const nonstd::span<const u8> pixels =
      m_vram.subspan(control.character_base_block());

  const u32 bits_per_pixel = control.bits_per_pixel();
  const u32 tile_length = bits_per_pixel * 8;
  const u32 tile_row_length = bits_per_pixel;

  const nonstd::span<const u8> palette = m_palette_ram;

  const auto tile_pixels_range =
      // Get tile map entries for the scanline
      vram.subspan(0, (ScreenWidth / TileSize) * 2) | ranges::view::chunk(2) |
      ranges::view::transform(
          [](auto pair) { return TileMapEntry(pair[0] | (pair[1] << 8)); }) |
      ranges::view::enumerate;

  for (const auto [index, entry] : tile_pixels_range) {
    const auto tile_offset = (tile_length * entry.tile_id()) +
                             ((scanline % TileSize) * tile_row_length);
    const auto tile_pixels = pixels.subspan(tile_offset, tile_row_length);
    const auto palette_bank = palette.subspan(
        2 * 16 * (bits_per_pixel == 4 ? entry.palette_bank() : 0));

    const auto base_index = index * TileSize;
    if (bits_per_pixel == 4) {
      render_tile_row_4bpp(m_framebuffer, palette_bank,
                           {static_cast<unsigned int>(base_index),
                            static_cast<unsigned int>(scanline)},
                           tile_pixels);
    } else {
      for (const auto [pixel_x, tile_group] :
           ranges::view::enumerate(tile_pixels)) {
        const u16 color = (palette_bank[tile_group * 2] << 0) |
                          (palette_bank[tile_group * 2 + 1] << 8);

        draw_color(m_framebuffer, base_index + pixel_x, scanline, color);
      }
    }
  }
}

void Gpu::render_sprites(int scanline) {
  auto range =
      m_oam_ram | ranges::view::chunk(8) |
      ranges::view::transform([](auto bytes) -> Sprite {
        return Sprite(bytes[0] | (bytes[1] << 8), bytes[2] | (bytes[3] << 8),
                      bytes[4] | (bytes[5] << 8));
      }) |
      ranges::view::filter([scanline](Sprite s) {
        return scanline >= s.attrib0.y() &&
               s.attrib0.mode() != ObjAttribute0::Mode::Disable;
      });

  const auto sprite_palette_ram = m_palette_ram.subspan(0x200);
  const auto sprite_tile_data = m_vram.subspan(0x010000);

  for (const auto sprite : range) {
    const auto tile_length = sprite.attrib0.bits_per_pixel() * 8;
    const auto tile_row_length = sprite.attrib0.bits_per_pixel();

    const auto sprite_rect =
        sprite_size(sprite.attrib0.shape(), sprite.attrib1.obj_size());
    const auto sprite_tile_rect = sprite_rect / TileSize;

    const auto sprite_y = sprite.attrib0.y();

    const auto sprite_height_scanline = ((scanline - sprite_y) / TileSize);

    const auto tile_scanline = ((scanline - sprite_y) % TileSize);
    if (scanline < sprite_y + sprite_rect.height) {
      // Render a scanline across all tiles
      const auto tile_base_offset = (sprite.attrib2.tile_id() * tile_length);
      for (unsigned int i = 0; i < sprite_tile_rect.width; ++i) {
        const auto sprite_pixels = sprite_tile_data.subspan(
            tile_base_offset + (sprite_height_scanline * 32 * tile_length) +
                (tile_row_length * tile_scanline) + (tile_length * i),

            tile_row_length);
        render_tile_row_4bpp(
            m_framebuffer,
            sprite_palette_ram.subspan(sprite.attrib2.palette_bank() * 2 * 16),
            {static_cast<unsigned int>(sprite.attrib1.x() + TileSize * i),
             static_cast<unsigned int>(scanline)},
            sprite_pixels);
      }
    }
  }
}
}  // namespace gb::advance
