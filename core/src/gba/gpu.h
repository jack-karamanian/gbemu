#pragma once
#include "color.h"
#include "error_handling.h"
#include "gba/mmu.h"
#include "static_vector.h"
#include "utils.h"

namespace gb::advance {
constexpr u32 TileSize = 8;
enum class BgMode : u32 { Zero = 0, One, Two, Three, Four, Five };

class Gpu;

class Dispcnt : public Integer<u16> {
 public:
  Dispcnt(Gpu& gpu) : Integer::Integer{0x0080}, m_gpu{&gpu} {}

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
    None = 0,
  };

  [[nodiscard]] bool layer_enabled(BackgroundLayer layer) const {
    return (static_cast<u16>(layer) & m_value) != 0;
  }

 private:
  Gpu* m_gpu;
};

class Bgcnt : public Integer<u16> {
 public:
  Bgcnt(Gpu& gpu) : Integer::Integer{0}, m_gpu{&gpu} {}

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
};

struct PriorityInfo {
  Dispcnt::BackgroundLayer layer;
  int priority;
};

class Bldcnt : public Integer<u16> {
 public:
  using Integer::Integer;

  enum class BlendMode : u16 {
    None = 0,
    Alpha = 1,
    BrightnessIncrease = 2,
    BrightnessDecrease = 3,
  };

  enum class BlendLayer : u16 {
    Background0 = 0,
    Background1 = 1,
    Background2 = 2,
    Background3 = 3,
    Obj = 4,
    Backdrop = 5,
    None = 6,
  };

  [[nodiscard]] BlendMode mode() const {
    return static_cast<BlendMode>((m_value >> 6) & 0b11);
  }

  [[nodiscard]] bool first_target_enabled(BlendLayer layer) const {
    assert(layer != BlendLayer::None);
    const auto layer_mask = 1 << static_cast<u16>(layer);
    return (m_value & layer_mask) != 0;
  }

  [[nodiscard]] bool second_target_enabled(BlendLayer layer) const {
    assert(layer != BlendLayer::None);
    const auto layer_mask = 1 << (static_cast<u16>(layer) + 8);
    return (m_value & layer_mask) != 0;
  }

  static BlendLayer to_blend_layer(Dispcnt::BackgroundLayer layer) {
    switch (layer) {
      case Dispcnt::BackgroundLayer::Zero:
        return BlendLayer::Background0;
      case Dispcnt::BackgroundLayer::One:
        return BlendLayer::Background1;
      case Dispcnt::BackgroundLayer::Two:
        return BlendLayer::Background2;
      case Dispcnt::BackgroundLayer::Three:
        return BlendLayer::Background3;
      case Dispcnt::BackgroundLayer::Obj:
        return BlendLayer::Obj;
      default:
#ifndef NDEBUG
        fmt::print("WARNING: BlendLayer::None reached\n");
#endif
        return BlendLayer::None;
    }
  }
};

class Bldalpha : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] float first_target_coefficient() const {
    return calculate_coefficient(m_value & 0b11111);
  }

  [[nodiscard]] float second_target_coefficient() const {
    return calculate_coefficient((m_value >> 8) & 0b11111);
  }

 private:
  static float calculate_coefficient(u16 value) {
    return value >= 16 ? 1.0F : (static_cast<float>(value) / 16.0F);
  }
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

  [[nodiscard]] bool disable() const { return test_bit(9); }

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

  Sprite() : attrib0{0}, attrib1{0}, attrib2{0} {}
};

struct Sprites {
  std::array<Sprite, 128> sprites{};
  unsigned int num_active_sprites = 0;
};

class Gpu {
 public:
  static constexpr u32 ScreenWidth = 240;
  static constexpr u32 ScreenHeight = 160;

  struct PerPixelContext {
    std::array<unsigned int, ScreenWidth> priorities;
    std::array<Color, ScreenWidth> top_pixels;
    std::array<StaticVector<Dispcnt::BackgroundLayer, 5>, ScreenWidth>
        pixel_layers;
    std::array<int, ScreenWidth> sprite_priorities;
    std::array<StaticVector<PriorityInfo, 5>, ScreenWidth> pixel_priorities;
    Dispcnt::BackgroundLayer blend_layer = Dispcnt::BackgroundLayer::Window0;
  };

  struct Background {
    Bgcnt control;
    Dispcnt::BackgroundLayer layer;
    Vec2<u16> scroll{0, 0};
    nonstd::span<Color> scanline;

    [[nodiscard]] int priority() const {
      return (static_cast<int>(layer) >> 8) + control.priority();
    }

    Background(Gpu& gpu,
               Dispcnt::BackgroundLayer l,
               nonstd::span<Color> scanline_buffer)
        : control{gpu}, layer{l}, scanline{scanline_buffer} {}
  };

  Gpu(Mmu& mmu)
      : m_vram{mmu.vram()},
        m_palette_ram{mmu.palette_ram()},
        m_oam_ram{mmu.oam_ram()},
        m_framebuffer(ScreenWidth * ScreenHeight) {}

  Dispcnt dispcnt{*this};

  std::array<std::array<Color, ScreenWidth>, 4> m_scanlines;

  Background bg0{*this, Dispcnt::BackgroundLayer::Zero, m_scanlines[0]};
  Background bg1{*this, Dispcnt::BackgroundLayer::One, m_scanlines[1]};
  Background bg2{*this, Dispcnt::BackgroundLayer::Two, m_scanlines[2]};
  Background bg3{*this, Dispcnt::BackgroundLayer::Three, m_scanlines[3]};

  Bldcnt bldcnt{0};
  Bldalpha bldalpha{0};

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

  void render_tile_row_4bpp(nonstd::span<Color> framebuffer,
                            nonstd::span<const u8> palette_bank,
                            Vec2<unsigned int> position,
                            unsigned int priority,
                            Gpu::PerPixelContext& per_pixel_context,
                            nonstd::span<const u8> tile_pixels,
                            // bool is_sprite,
                            Dispcnt::BackgroundLayer layer,
                            PriorityInfo priority_info,
                            bool horizontal_flip = false);
  void render_background(Background background, unsigned int scanline);
  void render_sprites(unsigned int scanline);

  nonstd::span<Color> framebuffer_from_layer(Dispcnt::BackgroundLayer layer) {
    switch (layer) {
      case Dispcnt::BackgroundLayer::Zero:
        return bg0.scanline;
      case Dispcnt::BackgroundLayer::One:
        return bg1.scanline;
      case Dispcnt::BackgroundLayer::Two:
        return bg2.scanline;
      case Dispcnt::BackgroundLayer::Three:
        return bg3.scanline;
      case Dispcnt::BackgroundLayer::Obj:
        return sprite_scanline;
    }
  }

  std::array<Background*, 4> m_backgrounds{&bg0, &bg1, &bg2, &bg3};
  decltype(m_backgrounds)::iterator m_backgrounds_end = m_backgrounds.begin();

  std::array<Color, ScreenWidth> sprite_scanline;

  nonstd::span<u8> m_vram;
  nonstd::span<u8> m_palette_ram;
  nonstd::span<u8> m_oam_ram;

  PerPixelContext m_per_pixel_context;
  std::vector<Color> m_framebuffer;
};
}  // namespace gb::advance
