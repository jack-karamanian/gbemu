#include "gba/gpu.h"
#include <doctest/doctest.h>
#include "gba/mmu.h"

namespace gb::advance {

template <typename T>
struct Mat2 {
  std::array<T, 4> data;
};

template <typename T>
constexpr Vec2<T> operator*(Mat2<T> mat, Vec2<T> vec) noexcept {
  return {mat.data[0] * vec.x + mat.data[1] * vec.y,
          mat.data[2] * vec.x + mat.data[3] * vec.y};
}

template <typename T>
constexpr Mat2<T> operator*(Mat2<T> lhs, Mat2<T> rhs) noexcept {
  return {
      lhs.data[0] * rhs.data[0] + lhs.data[1] * rhs.data[2],
      lhs.data[0] * rhs.data[1] + lhs.data[1] * rhs.data[3],
      lhs.data[2] * rhs.data[0] + lhs.data[3] * rhs.data[2],
      lhs.data[2] * rhs.data[1] + lhs.data[3] * rhs.data[3],
  };
}

template <typename T>
constexpr bool operator==(const Mat2<T>& lhs, const Mat2<T>& rhs) noexcept {
  return lhs.data[0] == rhs.data[0] && lhs.data[1] == rhs.data[1] &&
         lhs.data[2] == rhs.data[2] && lhs.data[3] == rhs.data[3];
}

using Mat2f = Mat2<float>;

static constexpr auto make_blend_func(float first_weight, float second_weight) {
  return [first_weight, second_weight](float channel1,
                                       float channel2) -> float {
    return std::min(255.0F, channel1 * first_weight + channel2 * second_weight);
  };
}

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

static std::size_t sprite_size_index(ObjAttribute0::Shape shape,
                                     unsigned int size_index) {
  const int shape_index = static_cast<int>(shape);

  return 3 * size_index + shape_index;
}

void Gpu::render_scanline(unsigned int scanline) {
  for (auto& infos : m_per_pixel_context.pixel_priorities) {
    infos.clear();
  }
  std::fill(m_per_pixel_context.sprite_priorities.begin(),
            m_per_pixel_context.sprite_priorities.end(), -1);
  for (auto& bg_scanline : m_scanlines) {
    std::fill(bg_scanline.begin(), bg_scanline.end(), Color{0, 0, 0, 0});
  }
  std::fill(m_framebuffer.begin() + ScreenWidth * scanline,
            m_framebuffer.begin() + ScreenWidth * scanline + ScreenWidth,
            Color{0, 0, 0, 255});
  std::fill(m_per_pixel_context.priorities.begin(),
            m_per_pixel_context.priorities.end(), -1);
  std::fill(m_per_pixel_context.top_pixels.begin(),
            m_per_pixel_context.top_pixels.end(), Color{0, 0, 0, 0});
  std::fill(sprite_scanline.begin(), sprite_scanline.end(), Color{0, 0, 0, 0});

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
      render_mode3(scanline);
      break;
    case BgMode::Four:
    case BgMode::Five:
      render_mode4(scanline);
      break;
  }
  render_sprites(scanline);

  {
    int x = 0;

    for (int priority : m_per_pixel_context.sprite_priorities) {
      if (priority != -1) {
        auto& priorities = m_per_pixel_context.pixel_priorities[x];
        const int insert_index = [&priorities, priority] {
          for (int i = priorities.size() - 1; i >= 0; --i) {
            if (priorities[i].priority >= priority) {
              return i + 1;
            }
          }
          return 0;
        }();
        priorities.insert(&priorities[insert_index],
                          PriorityInfo{Dispcnt::BackgroundLayer::Obj, -1});
      }
      ++x;
    }
  }

  if (bldcnt.mode() == Bldcnt::BlendMode::Alpha) {
    const float first_weight = bldalpha.first_target_coefficient();
    const float second_weight = bldalpha.second_target_coefficient();
    int x = 0;
    for (auto& pixel_layers : m_per_pixel_context.pixel_priorities) {
      auto first_target = std::find_if(
          pixel_layers.rbegin(), pixel_layers.rend(), [this](auto info) {
            return bldcnt.first_target_enabled(
                Bldcnt::to_blend_layer(info.layer));
          });

      auto second_target =

          std::find_if(first_target == pixel_layers.rend()
                           ? pixel_layers.rbegin()
                           : first_target + 1,
                       pixel_layers.rend(), [this](auto info) {
                         return bldcnt.second_target_enabled(
                             Bldcnt::to_blend_layer(info.layer));
                       });

      if ((first_target != pixel_layers.rend() &&
           second_target == pixel_layers.rend()) ||
          (first_target == pixel_layers.rend() &&
           second_target != pixel_layers.rend())) {
        m_framebuffer[ScreenWidth * scanline + x] =
            m_per_pixel_context.top_pixels[x];
      } else if (first_target != pixel_layers.rend() &&
                 (first_target + 1) != pixel_layers.rend()) {
        if (pixel_layers.back().layer == first_target->layer &&
            (first_target + 1)->layer == second_target->layer) {
          const auto blend = [first_weight, second_weight](float channel1,
                                                           float channel2) {
            return std::min(255.0F,
                            channel1 * first_weight + channel2 * second_weight);
          };
          const Color color_one =
              framebuffer_from_layer(first_target->layer)[x];
          const Color color_two =
              framebuffer_from_layer((first_target + 1)->layer)[x];

          m_framebuffer[ScreenWidth * scanline + x] = Color(
              blend(color_one.r, color_two.r), blend(color_one.g, color_two.g),
              blend(color_one.b, color_two.b), 255);
        } else {
          m_framebuffer[ScreenWidth * scanline + x] =
              m_per_pixel_context.top_pixels[x];
        }
      } else {
        m_framebuffer[ScreenWidth * scanline + x] =
            m_per_pixel_context.top_pixels[x];
      }
      ++x;
    }
  } else {
    const auto begin = m_per_pixel_context.top_pixels.cbegin();
    const auto end = m_per_pixel_context.top_pixels.cend();
    std::copy(begin, end, &m_framebuffer[ScreenWidth * scanline]);
  }
  for (auto& layers : m_per_pixel_context.pixel_layers) {
    layers.clear();
  }
}

void Gpu::sort_backgrounds() {
  auto i = std::partition(m_backgrounds.begin(), m_backgrounds.end(),
                          [this](auto background) {
                            return dispcnt.layer_enabled(background->layer);
                          });
  constexpr_sort(m_backgrounds.begin(), i, [](auto* a, auto* b) {
    const auto a_priority = a->control.priority();
    const auto b_priority = b->control.priority();
    if (a_priority == b_priority) {
      return static_cast<int>(a->layer) > static_cast<int>(b->layer);
    }
    return a_priority > b_priority;
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

static void render_tile_pixel_4bpp(nonstd::span<Color> framebuffer,
                                   nonstd::span<const u8> palette_bank,
                                   nonstd::span<const u8> tile_pixels,
                                   Gpu::PerPixelContext& per_pixel_context,
                                   Dispcnt::BackgroundLayer layer,
                                   unsigned int priority,
                                   unsigned int screen_x,
                                   unsigned int tile_x

) {
  const auto new_priority =
      layer == Dispcnt::BackgroundLayer::Obj ? -1 : priority;
  const auto tile_offset = 32 * (tile_x / TileSize);
  const u8 pixel = (tile_pixels[tile_offset + ((tile_x % TileSize) / 2)] >>
                    (4 * (tile_x & 1))) &
                   0xf;

  const u16 color =
      palette_bank[pixel * 2] | (palette_bank[pixel * 2 + 1] << 8);

  if (pixel != 0 && screen_x < Gpu::ScreenWidth) {
    const bool is_higher_priority =
        priority <= per_pixel_context.priorities[screen_x];
    draw_color(framebuffer, screen_x, 0, color);

    if (layer == Dispcnt::BackgroundLayer::Obj) {
      per_pixel_context.sprite_priorities[screen_x] = priority;
    }

    if (is_higher_priority) {
      auto& priorities = per_pixel_context.pixel_priorities[screen_x];
      if (layer != Dispcnt::BackgroundLayer::Obj &&
          (priorities.size() == 0 || priorities.back().layer != layer)) {
        priorities.emplace_back(layer, static_cast<int>(priority));
      }
      per_pixel_context.priorities[screen_x] = new_priority;
      per_pixel_context.top_pixels[screen_x] = framebuffer[screen_x];
    }
    // draw_color(framebuffer, screen_x, 0, color);
  }
}

void Gpu::render_tile_row_4bpp(nonstd::span<Color> framebuffer,
                               nonstd::span<const u8> palette_bank,
                               Vec2<unsigned int> position,
                               unsigned int priority,
                               Gpu::PerPixelContext& per_pixel_context,
                               nonstd::span<const u8> tile_pixels,
                               Dispcnt::BackgroundLayer layer,
                               PriorityInfo priority_info,
                               bool horizontal_flip

) {
  const auto [base_index, scanline] = position;

  const int pixel_two_offset = horizontal_flip ? -1 : 1;
  const unsigned int flip_target_offset = horizontal_flip ? 1 : 0;

  const auto render_pixel =
      [framebuffer, priority, priority_info, layer, &per_pixel_context,
       new_priority = layer == Dispcnt::BackgroundLayer::Obj ? -1 : priority](
          u16 color, unsigned int x) {
        if (x < Gpu::ScreenWidth) {
          const bool is_higher_priority =
              priority <= per_pixel_context.priorities[x];
          draw_color(framebuffer, x, 0, color);

          if (layer == Dispcnt::BackgroundLayer::Obj) {
            per_pixel_context.sprite_priorities[x] = priority;
          }

          if (is_higher_priority) {
            auto& priorities = per_pixel_context.pixel_priorities[x];
            if (layer != Dispcnt::BackgroundLayer::Obj &&
                (priorities.size() == 0 || priorities.back().layer != layer)) {
              priorities.push_back(priority_info);
            }
            per_pixel_context.priorities[x] = new_priority;
            per_pixel_context.top_pixels[x] = framebuffer[x];
          }
        }
      };

  for (unsigned int x = 0; x < 4; ++x) {
    const u8 tile_group = tile_pixels[x];
    const unsigned int pixel_x = horizontal_flip ? 4 - x - 1 : x;

    const u8 pixel_one = tile_group & 0xf;
    const u8 pixel_two = (tile_group >> 4) & 0xf;

    const u16 color_one =
        (palette_bank[pixel_one * 2] | (palette_bank[pixel_one * 2 + 1] << 8));
    const u16 color_two =
        (palette_bank[pixel_two * 2] | (palette_bank[pixel_two * 2 + 1] << 8));

    // Target 0..7 or 7..0 when flipped
    const auto target_pixel_x = pixel_x * 2 + flip_target_offset;

    if (pixel_one != 0) {
      const auto pixel_one_x = base_index + target_pixel_x;
      render_pixel(color_one, pixel_one_x);
    }

    if (pixel_two != 0) {
      const auto pixel_two_x = base_index + pixel_two_offset + target_pixel_x;
      render_pixel(color_two, pixel_two_x);
    }
  }
}

void Gpu::render_mode3(unsigned int scanline) {
  for (unsigned int x = 0; x < ScreenWidth; ++x) {
    const auto color_index = (ScreenWidth * scanline + x) * 2;
    const auto color = (m_vram[color_index] | (m_vram[color_index + 1] << 8));
    draw_color(m_per_pixel_context.top_pixels, x, 0, color);
  }
}

void Gpu::render_mode4(unsigned int scanline) {
  for (unsigned int x = 0; x < ScreenWidth; ++x) {
    const auto color_index = ScreenWidth * scanline + x;
    const auto palette_index = m_vram[color_index] * 2;
    const u16 color =
        m_palette_ram[palette_index] | (m_palette_ram[palette_index + 1] << 8);
    draw_color(m_per_pixel_context.top_pixels, x, 0, color);
  }
}

void Gpu::render_background(Background background, unsigned int scanline) {
  const u32 tile_scanline = scanline / TileSize;
  const u16 tile_x = (background.scroll.x & 0b1'1111'1111) / TileSize;
  const u16 tile_y = (background.scroll.y & 0b1'1111'1111) / TileSize;

  const Bgcnt control = background.control;

  const auto base_block_offset = control.tilemap_base_block();
  const nonstd::span<const u8> vram = m_vram.subspan(base_block_offset);

  const nonstd::span<const u8> pixels =
      m_vram.subspan(control.character_base_block());

  const u32 bits_per_pixel = control.bits_per_pixel();
  const u32 tile_length = bits_per_pixel * 8;
  const u32 tile_row_length = bits_per_pixel;

  const nonstd::span<const u8> palette = m_palette_ram;

  const auto get_tile_byte =
      [vram, screen_size_mode = background.control.screen_size_mode()](
          unsigned int x, unsigned int y, unsigned int offset) {
        const auto y_screen_offset =
            0;  //((y + tile_scanline) / tile_height) % (tile_height / 32);
        // const auto x_screen_offset = ((x) / tile_width) % (tile_width /
        // 32);

        const auto x_screen_offset = 0x7c0 * [screen_size_mode, x, offset] {
          switch (screen_size_mode) {
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

        const auto background_row_offset = [screen_size_mode] {
          switch (screen_size_mode) {
            case 0:
            case 2:
              return 0x0;
            case 1:
            case 3:
              return 0x800;
            default:
              GB_UNREACHABLE();
          }
        }();

        const auto map_width = [screen_size_mode] {
          switch (screen_size_mode) {
            case 0:
            case 2:
              return 32;
            case 1:
            case 3:
              return 64;
          }
          GB_UNREACHABLE();
        }();

        // The amount to subtract from the end of a row across all
        // backgrounds to get to the start of the row
        const auto background_row_width = background_row_offset + 0x40;

        const auto tile_row_start = (64 * (y % 32));
        const auto tile_row_end =
            tile_row_start + background_row_offset /*0x800*/ + 32 * 2;
        const auto tile_byte_offset = ((32 * (y % 32) + (x % map_width)) * 2) +
                                      (0x800 * y_screen_offset) +
                                      x_screen_offset + offset;

        return vram[tile_byte_offset >= tile_row_end
                        ? tile_byte_offset - background_row_width
                        : tile_byte_offset];
      };

  const auto tile_scroll_offset =
      tile_y + tile_scanline +
      // Render the next tile if the scanline is past the
      // midpoint created by scroll y
      ((scanline % TileSize) > (7 - (background.scroll.y % TileSize)) ? 1 : 0);

  const auto get_byte = [get_tile_byte, tile_x,
                         tile_scroll_offset](unsigned int i) {
    return get_tile_byte(tile_x, tile_scroll_offset, i);
  };

  // Render the number of tiles that can fit on the screen + 1 for scrolling
  for (unsigned int i = 0; i < ((ScreenWidth / TileSize) + 1) * 2; i += 2) {
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
      render_tile_row_4bpp(
          background.scanline, palette_bank,
          {base_index - (background.scroll.x % TileSize), 0 /*scanline*/},
          background.control.priority(), m_per_pixel_context, tile_pixels,
          background.layer,
          {background.layer, static_cast<int>(background.control.priority())},
          entry.horizontal_flip());
    } else {
      for (unsigned int pixel_x = 0; pixel_x < tile_pixels.size(); ++pixel_x) {
        const u8 tile_group = tile_pixels[pixel_x];
        const u16 color = (palette_bank[tile_group * 2]) |
                          (palette_bank[tile_group * 2 + 1] << 8);

        const auto x = entry.horizontal_flip()
                           ? (TileSize - 1) + base_index - pixel_x
                           : base_index + pixel_x;

        if (x < Gpu::ScreenWidth && tile_group != 0) {
          m_per_pixel_context.priorities[x] = background.control.priority();
          draw_color(m_per_pixel_context.top_pixels, x, 0 /*scanline*/, color);
        }
      }
    }
  }
}

void Gpu::render_sprites(unsigned int scanline) {
  const auto sprite_palette_ram = m_palette_ram.subspan(0x200);
  const auto sprite_tile_data = m_vram.subspan(0x010000);

  const float first_weight = bldalpha.first_target_coefficient();
  const float second_weight = bldalpha.second_target_coefficient();
  const auto blend = make_blend_func(first_weight, second_weight);
  // Render sprites backwards to express the priority
  std::array<Color, 128> old_tile;
  for (auto i = m_oam_ram.ssize() - 8; i >= 0; i -= 8) {
    const Sprite sprite(m_oam_ram[i + 0] | (m_oam_ram[i + 1] << 8),
                        m_oam_ram[i + 2] | (m_oam_ram[i + 3] << 8),
                        m_oam_ram[i + 4] | (m_oam_ram[i + 5] << 8));

    const auto mode = sprite.attrib0.mode();
    if (mode == ObjAttribute0::Mode::Disable || scanline < sprite.attrib0.y()) {
      continue;
    }

    const auto bits_per_pixel = sprite.attrib0.bits_per_pixel();
    const auto tile_length = bits_per_pixel * 8;
    const auto tile_row_length = bits_per_pixel;

    const auto sprite_rect_index =
        sprite_size_index(sprite.attrib0.shape(), sprite.attrib1.obj_size());

    const auto sprite_rect = [&sprite,
                              sprite_rect_index]() -> Rect<unsigned int> {
      const auto rect = sprite_sizes[sprite_rect_index];
      if (sprite.attrib0.mode() == ObjAttribute0::Mode::AffineDoubleRendering) {
        return {rect.width * 2, rect.height * 2};
      }
      return rect;
    }();

    const auto palette_ram =
        sprite_palette_ram.subspan(sprite.attrib2.palette_bank() * 2 * 16);

    static constexpr Mat2f identity{1, 0, 0, 1};
    static constexpr Mat2f scale_half{2, 0, 0, 2};

    const Mat2f matrix = [&]() -> Mat2f {
      if (mode == ObjAttribute0::Mode::Normal) {
        return identity;
      }
      const auto offset = 0x20 * sprite.attrib1.affine_parameter_group();
      const s16 pa =
          m_oam_ram[0x06 + offset] | (m_oam_ram[0x06 + offset + 1] << 8);
      const s16 pb =
          m_oam_ram[0x0e + offset] | (m_oam_ram[0x0e + offset + 1] << 8);
      const s16 pc =
          m_oam_ram[0x16 + offset] | (m_oam_ram[0x16 + offset + 1] << 8);
      const s16 pd =
          m_oam_ram[0x1e + offset] | (m_oam_ram[0x1e + offset + 1] << 8);

      const auto to_float = [](s16 fixed) {
        return static_cast<float>(fixed) / (1 << 8);
      };

      return Mat2f{to_float(pa), to_float(pb), to_float(pc), to_float(pd)} *
             (mode == ObjAttribute0::Mode::AffineDoubleRendering ? scale_half
                                                                 : identity);
    }();

    const int coord_divisor =
        static_cast<int>(mode == ObjAttribute0::Mode::AffineDoubleRendering);

    const Rect<unsigned int> half_rect{sprite_rect.width >> coord_divisor,
                                       sprite_rect.height >> coord_divisor};

    constexpr auto lerp = [](float v0, float v1, float t) -> float {
      return (1.0F - t) * v0 + t * v1;
    };

    for (unsigned int x = 0; x < sprite_rect.width; ++x) {
      // The transform occurs at the center of the sprite
      const auto [transformed_scanline, transformed_x] =
          [&]() -> std::tuple<unsigned int, unsigned int> {
        if (mode == ObjAttribute0::Mode::Normal) {
          return {scanline, x};
        }
        const Vec2<float> vec =
            matrix *
            Vec2<float>{
                lerp(-1.0F, 1.0F, static_cast<float>(x) / sprite_rect.width),
                lerp(-1.0F, 1.0F,
                     static_cast<float>((scanline - sprite.attrib0.y())) /
                         sprite_rect.height)};

        const auto new_scanline =
            static_cast<unsigned int>((vec.y + 1.0F) * 0.5F *
                                      static_cast<float>(half_rect.height)) +
            sprite.attrib0.y();
        const auto new_x = static_cast<unsigned int>(
            (vec.x + 1.0F) * 0.5F * static_cast<float>(half_rect.width));

        return {new_scanline, new_x};
      }();

      const auto tile_base_offset = (sprite.attrib2.tile_id() * tile_length);
      const auto sprite_y = static_cast<unsigned int>(sprite.attrib0.y());

      // The scanline in sprite y coordinates
      const auto scanline_relative_to_sprite =
          sprite.attrib1.vertical_flip()
              ? sprite_rect.height - (transformed_scanline - sprite_y) - 1
              : (transformed_scanline - sprite_y);

      const auto sprite_height_scanline =
          scanline_relative_to_sprite / TileSize;

      const auto tile_scanline = (scanline_relative_to_sprite % TileSize);

      if (transformed_scanline < sprite_y) {
        continue;
      }
      if (transformed_scanline >= sprite_y + half_rect.height) {
        continue;
      }

      const auto sprite_2d_offset =
          dispcnt.obj_vram_mapping() == Dispcnt::ObjVramMapping::TwoDimensional
              ? (sprite_height_scanline * 32 * tile_length)
              : (sprite_height_scanline * half_rect.width * tile_row_length);

      const auto sprite_pixels =
          sprite_tile_data.subspan(tile_base_offset + sprite_2d_offset +
                                   (tile_row_length * tile_scanline)

          );

      const auto base_x = sprite.attrib1.x() + (x);
      const auto reversed_x = sprite.attrib1.horizontal_flip()
                                  ? half_rect.width - transformed_x - 1
                                  : transformed_x;

      if (reversed_x >= half_rect.width) {
        continue;
      }

      if (base_x < ScreenWidth) {
        old_tile[x] = m_per_pixel_context.top_pixels[base_x];
      }
      assert(reversed_x < sprite_rect.width);

      render_tile_pixel_4bpp(sprite_scanline, palette_ram, sprite_pixels,
                             m_per_pixel_context, Dispcnt::BackgroundLayer::Obj,
                             sprite.attrib2.priority(), base_x, reversed_x);

      if (sprite.attrib0.gfx_mode() == ObjAttribute0::GfxMode::AlphaBlending &&
          base_x < ScreenWidth) {
        // Disable any other alpha blending for this pixel
        // and write the blended sprite
        m_per_pixel_context.pixel_priorities[x].clear();
        const Color color_one = m_per_pixel_context.top_pixels[base_x];
        const Color color_two = old_tile[x];
        m_per_pixel_context.top_pixels[base_x] = Color(
            blend(color_one.r, color_two.r), blend(color_one.g, color_two.g),
            blend(color_one.b, color_two.b), 255);
      }
    }
  }
}  // namespace gb::advance

TEST_CASE("matrix multiplication") {
  constexpr Mat2<int> mat{{3, 5, 5, 2}};
  constexpr Vec2<int> vec{3, 4};

  constexpr auto res = (mat * vec);
  CAPTURE(res.x);
  CAPTURE(res.y);
  CHECK((res == Vec2<int>{29, 23}));
}
}  // namespace gb::advance
