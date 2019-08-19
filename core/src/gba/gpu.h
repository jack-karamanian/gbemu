#pragma once
#include "color.h"
#include "error_handling.h"
#include "gba/mmu.h"
#include "utils.h"

namespace gb::advance {
constexpr u32 TileSize = 8;
enum class BgMode : u32 { Zero = 0, One, Two, Three, Four, Five };
class Dispcnt : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] BgMode bg_mode() const {
    return static_cast<BgMode>(m_value & 0b111);
  }

  [[nodiscard]] bool cgb_mode() const { return test_bit(3); }
  enum class DisplayFrame : u32 {
    Back = 0,
    Front,
  };

  [[nodiscard]] DisplayFrame display_frame() const {
    return static_cast<DisplayFrame>((m_value >> 4) & 0b1);
  }

  enum class ObjVramMapping : u32 { TwoDimensional = 0, OneDimensional };

  [[nodiscard]] ObjVramMapping obj_vram_mapping() const {
    return static_cast<ObjVramMapping>((m_value >> 6) & 0b1);
  }

  enum class BackgroundLayer : u16 {
    Zero = 1 << 8,
    One = 1 << 9,
    Two = 1 << 10,
    Three = 1 << 11,
    Obj = 1 << 12,
    Window0 = 1 << 13,
    Window1 = 1 << 14,
    ObjWindow = 1 << 15,
  };

  [[nodiscard]] bool layer_enabled(BackgroundLayer layer) const {
    return (static_cast<u16>(layer) & m_value) != 0;
  }
};

class Bgcnt : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] u32 priority() const { return m_value & 0b11; }

  [[nodiscard]] u32 character_base_block() const {
    return ((m_value >> 2) & 0b11) * 16_kb;
  }

  [[nodiscard]] bool mosaic() const { return test_bit(6); }

  struct ColorsAndPalettes {
    u32 colors;
    u32 palettes;
  };

  [[nodiscard]] u32 bits_per_pixel() const { return test_bit(7) ? 8 : 4; }

  [[nodiscard]] ColorsAndPalettes colors_and_palettes() const {
    return test_bit(7) ? ColorsAndPalettes{256, 1} : ColorsAndPalettes{16, 16};
  }

  [[nodiscard]] u32 tilemap_base_block() const {
    return ((m_value >> 8) & 0b11111) * 2_kb;
  }

  enum class DisplayOverflow {
    Transparent,
    Wraparound,
  };

  [[nodiscard]] DisplayOverflow display_overflow() const {
    return test_bit(13) ? DisplayOverflow::Transparent
                        : DisplayOverflow::Wraparound;
  }

  struct ScreenSize {
    Rect<u32> screen_size;
    Rect<u32> rotation_scaling_size;

    [[nodiscard]] ScreenSize as_tiles() const {
      return {screen_size / TileSize, rotation_scaling_size / TileSize};
    }
  };

  [[nodiscard]] ScreenSize screen_size() const {
    return [this]() -> ScreenSize {
      switch ((m_value >> 14) & 0b11) {
        case 0b00:
          return {{256, 256}, {128, 128}};
        case 0b01:
          return {{512, 256}, {256, 256}};
        case 0b10:
          return {{256, 512}, {512, 512}};
        case 0b11:
          return {{512, 512}, {1024, 1024}};
      }
      GB_UNREACHABLE();
    }();
  }
};

struct Background {
  Bgcnt control{0};
  Vec2<u16> scroll{0, 0};
};

class Gpu {
 public:
  static constexpr u32 ScreenWidth = 240;
  static constexpr u32 ScreenHeight = 160;

  Gpu(Mmu& mmu)
      : m_vram{mmu.vram()},
        m_palette_ram{mmu.palette_ram()},
        m_framebuffer(ScreenWidth * ScreenHeight) {}

  Dispcnt dispcnt{0};
  Background bg0;
  Background bg1;
  Background bg2;
  Background bg3;

  void render_scanline(int scanline);

  void render();

  [[nodiscard]] nonstd::span<const Color> framebuffer() const {
    return {m_framebuffer};
  }

 private:
  void render_background(Background background, int scanline);
  nonstd::span<u8> m_vram;
  nonstd::span<u8> m_palette_ram;
  std::vector<Color> m_framebuffer;
};
}  // namespace gb::advance
