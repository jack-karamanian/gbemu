#include "gba/gpu.h"
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
  [[nodiscard]] bool horizontal_flip() const { return test_bit(10); };
  [[nodiscard]] bool vertical_flip() const { return test_bit(11); }
  [[nodiscard]] int palette_bank() const { return (m_value >> 12) & 0xf; }
};

class ObjAttribute0 : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] unsigned int y() const { return m_value & 0xff; }

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

  [[nodiscard]] unsigned int bits_per_pixel() const {
    return test_bit(13) ? 8 : 4;
  }

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
  [[nodiscard]] unsigned int x() const { return m_value & 0b1'1111'1111; }

  [[nodiscard]] bool horizontal_flip() const { return test_bit(12); }

  [[nodiscard]] bool vertical_flip() const { return test_bit(13); }

  [[nodiscard]] unsigned int obj_size() const { return (m_value >> 14) & 0b11; }
};

class ObjAttribute2 : public Integer<u16> {
 public:
  using Integer::Integer;
  [[nodiscard]] unsigned int tile_id() const {
    return m_value & 0b11'1111'1111;
  }

  [[nodiscard]] unsigned int priority() const { return (m_value >> 10) & 0b11; }

  [[nodiscard]] unsigned int palette_bank() const {
    return (m_value >> 12) & 0b1111;
  }
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
                                      unsigned int size_index) {
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

void Gpu::render_scanline(unsigned int scanline) {
  std::fill(m_framebuffer.begin() + ScreenWidth * scanline,
            m_framebuffer.begin() + ScreenWidth * scanline + ScreenWidth,
            Color{0, 0, 0, 255});

  switch (dispcnt.bg_mode()) {
    case BgMode::Zero:
    case BgMode::One:
    case BgMode::Two:
      for (auto background = m_backgrounds.begin();
           background != m_backgrounds_end; ++background) {
        render_background(**background, scanline);
      }
      break;
    case BgMode::Three:
    case BgMode::Four:
    case BgMode::Five:
      render_mode4();
      break;
  }
  render_sprites(scanline);
}

void Gpu::sort_backgrounds() {
  auto i = std::partition(m_backgrounds.begin(), m_backgrounds.end(),
                          [this](auto background) {
                            return dispcnt.layer_enabled(background->layer);
                          });
  constexpr_sort(m_backgrounds.begin(), i, [](auto* a, auto* b) {
    auto a_priority = a->control.priority();
    auto b_priority = b->control.priority();
    if (a_priority == b_priority) {
      return static_cast<int>(a->layer) > static_cast<int>(b->layer);
    }
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
                                 nonstd::span<const u8> tile_pixels,
                                 bool horizontal_flip = false) {
  const auto [base_index, scanline] = position;

  const unsigned int pixel_two_offset = horizontal_flip ? -1 : 1;
  for (unsigned int x = 0; x < tile_pixels.size(); ++x) {
    const u8 tile_group = tile_pixels[x];
    const auto pixel_x = horizontal_flip ? tile_pixels.size() - x - 1 : x;

    const u8 pixel_one = tile_group & 0xf;
    const u8 pixel_two = (tile_group >> 4) & 0xf;

    const u16 color_one =
        (palette_bank[pixel_one * 2] | (palette_bank[pixel_one * 2 + 1] << 8));
    const u16 color_two =
        (palette_bank[pixel_two * 2] | (palette_bank[pixel_two * 2 + 1] << 8));

    if (pixel_one != 0) {
      draw_color(framebuffer, (base_index + pixel_x * 2) % Gpu::ScreenWidth,
                 scanline, color_one);
    }
    if (pixel_two != 0) {
      draw_color(
          framebuffer,
          (base_index + pixel_two_offset + pixel_x * 2) % Gpu::ScreenWidth,
          scanline, color_two);
    }
  }
}

void Gpu::render_mode4() {
  // const u32 frame_offset =
  //    static_cast<u32>(dispcnt.display_frame()) * ScreenWidth *
  //    ScreenHeight;
  const auto background = m_vram.subspan(0, ScreenHeight * ScreenWidth);

  for (unsigned int y = 0; y < ScreenHeight; ++y) {
    for (unsigned int x = 0; x < ScreenWidth; ++x) {
      const auto color_index = ScreenWidth * y + x;
      const auto palette_index = m_vram[color_index] * 2;
      const u16 color = m_palette_ram[palette_index] |
                        (m_palette_ram[palette_index + 1] << 8);
      draw_color(m_framebuffer, x, y, color);
    }
  }
}

void Gpu::render_background(Background background, unsigned int scanline) {
  const u32 tile_scanline = scanline / TileSize;
#if 0
  const s16 tile_x = static_cast<s16>(background.scroll.x) / TileSize;
  const s16 tile_y = static_cast<s16>(background.scroll.y) / TileSize;
#endif
  const u16 tile_x = (background.scroll.x & 0b1'1111'1111) / TileSize;
  const u16 tile_y = (background.scroll.y & 0b1'1111'1111) / TileSize;

  const Bgcnt control = background.control;

  const auto screen_size = control.screen_size();

#if 0
  const auto tile_screen_size = screen_size.as_tiles();
  const u32 scroll_offset = [&] {
    const auto total_length = tile_screen_size.screen_size.width *
                              tile_screen_size.screen_size.height * 2;
    const auto base_offset =
        (32 * tile_y + tile_x) * 2 + (32 * 2 * tile_scanline);

    return control.display_overflow() == Bgcnt::DisplayOverflow::Wraparound
               ? base_offset % total_length
               : base_offset;
  }();
#endif

  const auto base_block_offset = control.tilemap_base_block();
  const nonstd::span<const u8> vram = m_vram.subspan(base_block_offset);

  const nonstd::span<const u8> pixels =
      m_vram.subspan(control.character_base_block());

  const u32 bits_per_pixel = control.bits_per_pixel();
  const u32 tile_length = bits_per_pixel * 8;
  const u32 tile_row_length = bits_per_pixel;

  const nonstd::span<const u8> palette = m_palette_ram;

  const auto get_tile_byte = [vram, background](unsigned int x, unsigned int y,
                                                unsigned int offset) {
    const auto y_screen_offset =
        0;  //((y + tile_scanline) / tile_height) % (tile_height / 32);
    // const auto x_screen_offset = ((x) / tile_width) % (tile_width / 32);

    const auto x_screen_offset =
        0x7c0 * [control = background.control, x, offset] {
          switch (control.screen_size_mode()) {
            case 0:
              return 0;
            case 1: {
              const auto tile_index = x + offset / 2;
              return tile_index > 31 ? 1 : 0;
            }
            case 2:
              return 0;
            case 3:
              return 0;
            default:
              GB_UNREACHABLE();
          }
        }();

    const auto tile_row_start = (32 * (y % 32) * 2);
    const auto tile_row_end = tile_row_start + 0x800 + 32 * 2;
    const auto tile_byte_offset = ((32 * (y % 32) + x) * 2) +
                                  (0x800 * y_screen_offset) + x_screen_offset +
                                  offset;

    if (background.control.screen_size_mode() == 1) {
      return vram[tile_byte_offset >= tile_row_end ? tile_byte_offset - 0x840
                                                   : tile_byte_offset];
    }
    return vram[tile_byte_offset];
  };

  const auto tile_scroll_offset =
      tile_y + tile_scanline +
      // Render the next tile if the scanline is past the
      // midpoint created by scroll y
      ((scanline % 8) > (7 - (background.scroll.y % TileSize)) ? 1 : 0);
  const auto get_byte = [get_tile_byte, tile_x, tile_scroll_offset](int i) {
    return get_tile_byte(tile_x, tile_scroll_offset, i);
  };

  for (unsigned int i = 0; i < (ScreenWidth / TileSize) * 2; i += 2) {
    const unsigned int index = i / 2;
    const TileMapEntry entry(get_byte(i) | (get_byte(i + 1) << 8));

    const auto scanline_offset = [scanline, entry,
                                  scroll_y = background.scroll.y] {
      const auto offset_scanline = ((scanline + scroll_y) % TileSize);
      return entry.vertical_flip() ? TileSize - offset_scanline - 1
                                   : offset_scanline;
    }();

    const auto tile_offset =
        (tile_length * entry.tile_id()) + (scanline_offset * tile_row_length);
    const auto tile_pixels = pixels.subspan(tile_offset, tile_row_length);
    const auto palette_bank = palette.subspan(
        2 * 16 * (bits_per_pixel == 4 ? entry.palette_bank() : 0));

    const auto base_index = index * TileSize;
    if (bits_per_pixel == 4) {
      render_tile_row_4bpp(m_framebuffer, palette_bank,
                           {static_cast<unsigned int>(
                                base_index - (background.scroll.x % TileSize)),
                            static_cast<unsigned int>(scanline)},
                           tile_pixels, entry.horizontal_flip());
    } else {
      for (unsigned int pixel_x = 0; pixel_x < tile_pixels.size(); ++pixel_x) {
        const u8 tile_group = tile_pixels[pixel_x];
        const u16 color = (palette_bank[tile_group * 2]) |
                          (palette_bank[tile_group * 2 + 1] << 8);

        draw_color(m_framebuffer,
                   entry.horizontal_flip() ? TileSize + base_index - pixel_x
                                           : base_index + pixel_x,
                   scanline, color);
      }
    }
  }
}

void Gpu::render_sprites(unsigned int scanline) {
  const auto sprite_palette_ram = m_palette_ram.subspan(0x200);
  const auto sprite_tile_data = m_vram.subspan(0x010000);

  for (unsigned int i = 0; i < m_oam_ram.size(); i += 8) {
    const Sprite sprite(m_oam_ram[i + 0] | (m_oam_ram[i + 1] << 8),
                        m_oam_ram[i + 2] | (m_oam_ram[i + 3] << 8),
                        m_oam_ram[i + 4] | (m_oam_ram[i + 5] << 8));

    if (!(scanline >= sprite.attrib0.y() &&
          sprite.attrib0.mode() != ObjAttribute0::Mode::Disable)) {
      continue;
    }
    const auto bits_per_pixel = sprite.attrib0.bits_per_pixel();
    const auto tile_length = bits_per_pixel * 8;
    const auto tile_row_length = bits_per_pixel;

    const auto sprite_rect =
        sprite_size(sprite.attrib0.shape(), sprite.attrib1.obj_size());
    const auto sprite_tile_rect = sprite_rect / TileSize;

    const auto sprite_y = sprite.attrib0.y();

    const auto sprite_height_scanline = ((scanline - sprite_y) / TileSize);

    const auto tile_scanline = ((scanline - sprite_y) % TileSize);
    const auto sprite_2d_offset =
        dispcnt.obj_vram_mapping() == Dispcnt::ObjVramMapping::TwoDimensional
            ? (sprite_height_scanline * 32 * tile_length)
            : 0;

    if (scanline < sprite_y + sprite_rect.height) {
      const auto tile_base_offset = (sprite.attrib2.tile_id() * tile_length);

      const auto render_sprite_tile = [sprite_tile_data, sprite_palette_ram,
                                       sprite_tile_rect, tile_row_length,
                                       tile_scanline, tile_length, sprite,
                                       tile_base_offset, sprite_2d_offset,
                                       scanline, this](unsigned int index) {
        const auto sprite_pixels = sprite_tile_data.subspan(
            tile_base_offset + sprite_2d_offset +
                (tile_row_length * tile_scanline) + (tile_length * index),

            tile_row_length);

        const auto base_x =
            sprite.attrib1.horizontal_flip()
                ? (sprite.attrib1.x() +
                   TileSize * (sprite_tile_rect.width - index - 1))
                : (sprite.attrib1.x() + TileSize * index);

        render_tile_row_4bpp(
            m_framebuffer,
            sprite_palette_ram.subspan(sprite.attrib2.palette_bank() * 2 * 16),
            {static_cast<unsigned int>(base_x), scanline}, sprite_pixels,
            sprite.attrib1.horizontal_flip());
      };
      // Render a scanline across all tiles
      for (unsigned int index = 0; index < sprite_tile_rect.width; ++index) {
        render_sprite_tile(index);
      }
    }
  }
}
}  // namespace gb::advance
