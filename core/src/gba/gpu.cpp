#include <chrono>
#include <range/v3/all.hpp>
#include "gba/gpu.h"
#include "gba/mmu.h"

namespace gb::advance {
struct TileMapEntry : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] u32 tile_id() const { return m_value & 0b1'1111'1111; }
  [[nodiscard]] u32 palette_bank() const { return (m_value >> 12) & 0xf; }
};
void Gpu::render() {}

void Gpu::render_scanline(int scanline) {
  if (dispcnt.layer_enabled(Dispcnt::BackgroundLayer::Zero)) {
    auto start = std::chrono::high_resolution_clock::now();
    render_background(bg0, scanline);
    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    // fmt::print("render time {}\n", ns.count());
  }
  if (dispcnt.layer_enabled(Dispcnt::BackgroundLayer::One)) {
    // render_background(bg1, scanline);
  }
  if (dispcnt.layer_enabled(Dispcnt::BackgroundLayer::Two)) {
    // render_background(bg2, scanline);
  }
  if (dispcnt.layer_enabled(Dispcnt::BackgroundLayer::Three)) {
    // render_background(bg3, scanline);
  }
}

void Gpu::render_background(Background background, int scanline) {
  const u32 tile_scanline = scanline / TileSize;
  const u16 tile_x = background.scroll.x / TileSize;
  const u16 tile_y = background.scroll.y / TileSize;

  const Bgcnt control = background.control;

  const Bgcnt::ScreenSize screen_size = control.screen_size();
  const Bgcnt::ScreenSize tile_screen_size = screen_size.as_tiles();

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

  const auto add_color = [this, scanline](u16 color, u32 x) {
    m_framebuffer[ScreenWidth * scanline + x] = {
        static_cast<u8>(convert_space<32, 255>(color & 0x1f)),
        static_cast<u8>(convert_space<32, 255>((color >> 5) & 0x1f)),
        static_cast<u8>(convert_space<32, 255>((color >> 10) & 0x1f)), 255};
  };

  const auto tile_pixels_range =
      // Get tile map entries for the scanline
      vram | ranges::view::chunk(2) | ranges::view::transform([](auto pair) {
        return TileMapEntry(pair[0] | (pair[1] << 8));
      }) |
      ranges::view::take(ScreenWidth / TileSize) | ranges::view::enumerate;

  ranges::for_each(tile_pixels_range, [bits_per_pixel, pixels, palette,
                                       add_color, tile_row_length, tile_length,
                                       scanline](
                                          std::pair<int, TileMapEntry> pair) {
    const auto [index, entry] = pair;
    const auto tile_offset = (tile_length * entry.tile_id()) +
                             ((scanline % TileSize) * tile_row_length);
    const auto tile_pixels = pixels.subspan(tile_offset, tile_row_length);
    const auto palette_bank = palette.subspan(
        2 * 16 * (bits_per_pixel == 4 ? entry.palette_bank() : 0));

    if (bits_per_pixel == 4) {
      ranges::for_each(
          ranges::view::enumerate(tile_pixels),
          [add_color, palette_bank,
           base_index = index * TileSize](auto tile_pair) {
            const auto [pixel_x, tile_group] = tile_pair;
            const u8 pixel_one = (tile_group >> 4) & 0xf;
            const u8 pixel_two = tile_group & 0xf;

            const u16 color_one = palette_bank[pixel_one * 2] |
                                  (palette_bank[pixel_one * 2 + 1] << 8);
            const u16 color_two = palette_bank[pixel_two * 2] |
                                  (palette_bank[pixel_two * 2 + 1] << 8);

            assert(base_index + pixel_x < ScreenWidth);
            // assert(base_index + pixel_x + 1 < ScreenWidth);

            add_color(color_one, base_index + pixel_x * 2);
            add_color(color_two, (base_index + 1 + pixel_x * 2) % ScreenWidth);
          });
    } else {
      ranges::for_each(
          ranges::view::enumerate(tile_pixels),
          [palette_bank, add_color, base_index = index](auto tile_pair) {
            const auto [pixel_x, tile_group] = tile_pair;
            const u16 color = (palette_bank[tile_group] << 0) |
                              (palette_bank[tile_group + 1] << 8);

            add_color(color, base_index + pixel_x);
          });
    }
  });
}
}  // namespace gb::advance
