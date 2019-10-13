#pragma once
#include "color.h"
#include "error_handling.h"
#include "gba/mmu.h"
#include "utils.h"

namespace gb::advance {
constexpr u32 TileSize = 8;
enum class BgMode : u32 { Zero = 0, One, Two, Three, Four, Five };

class Gpu;

class Dispcnt : public Integer<u16> {
 public:
  Dispcnt(Gpu& gpu) : Integer::Integer{0}, m_gpu{&gpu} {}

  void on_after_write() const;

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

 private:
  Gpu* m_gpu;
};

class Bgcnt : public Integer<u16> {
 public:
  Bgcnt(Gpu& gpu, Dispcnt::BackgroundLayer layer)
      : Integer::Integer{0}, m_gpu{&gpu}, m_layer{layer} {}

  void on_after_write() const;

  [[nodiscard]] u32 priority() const { return m_value & 0b11; }

  [[nodiscard]] u32 character_base_block() const {
    return ((m_value >> 2) & 0b11) * 16_kb;
  }

  [[nodiscard]] bool mosaic() const { return test_bit(6); }

  [[nodiscard]] u32 bits_per_pixel() const { return test_bit(7) ? 8 : 4; }

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
    Rect<unsigned int> screen_size;
    Rect<unsigned int> rotation_scaling_size;

    [[nodiscard]] ScreenSize as_tiles() const {
      return {screen_size / TileSize, rotation_scaling_size / TileSize};
    }
  };

  [[nodiscard]] u32 screen_size_mode() const { return (m_value >> 14) & 0b11; }

  [[nodiscard]] ScreenSize screen_size() const {
    switch (screen_size_mode()) {
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
  }

 private:
  Gpu* m_gpu;
  Dispcnt::BackgroundLayer m_layer;
};

class Bldcnt : public Integer<u16> {
 public:
  using Integer::Integer;
};

class Gpu {
 public:
  static constexpr u32 ScreenWidth = 240;
  static constexpr u32 ScreenHeight = 160;

  struct Background {
    Bgcnt control;
    Dispcnt::BackgroundLayer layer;
    Vec2<u16> scroll{0, 0};

    Background(Gpu& gpu, Dispcnt::BackgroundLayer l)
        : control{gpu, l}, layer{l} {}
  };

  Gpu(Mmu& mmu)
      : m_vram{mmu.vram()},
        m_palette_ram{mmu.palette_ram()},
        m_oam_ram{mmu.oam_ram()},
        m_framebuffer(ScreenWidth * ScreenHeight) {}

  Dispcnt dispcnt{*this};

  Background bg0{*this, Dispcnt::BackgroundLayer::Zero};
  Background bg1{*this, Dispcnt::BackgroundLayer::One};
  Background bg2{*this, Dispcnt::BackgroundLayer::Two};
  Background bg3{*this, Dispcnt::BackgroundLayer::Three};
  u16 bldcnt = 0;

  void sort_backgrounds();

  void clear() {
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), Color{0, 0, 0, 255});
  }

  void render_scanline(unsigned int scanline);

  [[nodiscard]] nonstd::span<const Color> framebuffer() const {
    return {m_framebuffer};
  }

 private:
  void render_mode4();
  void render_background(Background background, unsigned int scanline);
  void render_sprites(unsigned int scanline);

  std::array<Background*, 4> m_backgrounds{&bg0, &bg1, &bg2, &bg3};
  std::array<Background*, 4>::iterator m_backgrounds_end =
      m_backgrounds.begin();

  nonstd::span<u8> m_vram;
  nonstd::span<u8> m_palette_ram;
  nonstd::span<u8> m_oam_ram;

  std::vector<Color> m_framebuffer;
};
}  // namespace gb::advance
