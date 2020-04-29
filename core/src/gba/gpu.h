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

enum class WindowId : u32 {
  Zero = 0,
  One = 1,
};

class Dispcnt : public Integer<u16> {
 public:
  Dispcnt(Gpu& gpu) : Integer::Integer{0x0080}, m_gpu{&gpu} {}

  void on_after_write() const;

  [[nodiscard]] BgMode bg_mode() const {
    return static_cast<BgMode>(m_value & 0b111);
  }

  [[nodiscard]] bool cgb_mode() const { return test_bit(3); }

  [[nodiscard]] bool display_front() const {
    return ((m_value >> 4) & 0b1) != 0;
  }

  [[nodiscard]] bool hblank_interval_free() const { return test_bit(5); }

  enum class ObjVramMapping : u32 { TwoDimensional = 0, OneDimensional };

  [[nodiscard]] ObjVramMapping obj_vram_mapping() const {
    return static_cast<ObjVramMapping>((m_value >> 6) & 0b1);
  }

  [[nodiscard]] bool forced_blank() const { return test_bit(7); }

  enum class BackgroundLayer : u16 {
    Zero = 0,  // 1 << 8,
    One = 1,
    Two = 2,
    Three = 3,
    Obj = 4,
    Window0 = 5,
    Window1 = 6,
    ObjWindow = 7,
    Backdrop,
    None = 0xffff,
  };

  [[nodiscard]] bool layer_enabled(BackgroundLayer layer) const {
    return test_bit(8 + static_cast<u16>(layer));
  }

  [[nodiscard]] bool layer_enabled(WindowId layer) const {
    return test_bit(13 + static_cast<u16>(layer));
  }

 private:
  Gpu* m_gpu;
};

class Bgcnt : public Integer<u16> {
 public:
  Bgcnt(Gpu& gpu) : Integer::Integer{0}, m_gpu{&gpu} {}

  void on_after_write() const;

  [[nodiscard]] u32 priority() const { return m_value & 0b11; }

  // Tile pixels location
  [[nodiscard]] u32 character_base_block() const {
    return ((m_value >> 2) & 0b11) * 16_kb;
  }

  [[nodiscard]] bool mosaic() const { return test_bit(6); }

  [[nodiscard]] u32 bits_per_pixel() const { return test_bit(7) ? 8 : 4; }

  // Tile data location
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
      case Dispcnt::BackgroundLayer::Backdrop:
        return BlendLayer::Backdrop;
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

  [[nodiscard]] int affine_parameter_group() const {
    return (m_value >> 9) & 0b11111;
  }

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

class Bldy : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] float coefficient() const {
    return std::clamp(m_value & 0b11111, 0, 16) / 16.0F;
  }

  [[nodiscard]] bool is_one() const { return m_value >= 0x10; }

  [[nodiscard]] bool is_zero() const { return m_value == 0; }
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

class WindowIn : public Integer<u16> {
 public:
  WindowIn() : Integer::Integer{0} {}

  [[nodiscard]] bool should_display_layer(
      WindowId window,
      Dispcnt::BackgroundLayer layer) const {
    return test_bit(static_cast<u16>(layer) + static_cast<u32>(window) * 8);
  }

  [[nodiscard]] bool enable_blend_effects(WindowId window) const {
    return test_bit(5 + (window == WindowId::One ? 8 : 0));
  }

  [[nodiscard]] u8 enabled_layer_bits(WindowId window) const {
    return (m_value >> (window == WindowId::One ? 8 : 0)) & 0xFF;
  }
};

class WindowOut : public Integer<u16> {
 public:
  WindowOut() : Integer::Integer{0} {}

  [[nodiscard]] bool should_display_layer(
      Dispcnt::BackgroundLayer layer) const {
    return test_bit(static_cast<u16>(layer));
  }

  [[nodiscard]] bool enable_blend_effects(WindowId window) const {
    return test_bit(5 + (window == WindowId::One ? 8 : 0));
  }

  [[nodiscard]] u8 enabled_layer_bits() const { return m_value & 0xFF; }
};

class WindowBounds : public Integer<u16> {
 public:
  WindowBounds() : Integer::Integer{0} {}

  void write_byte(unsigned int byte, u8 value) {
    Integer::write_byte(byte, value);

    switch (byte) {
      case 0:
        m_max = value;
        break;
      case 1:
        m_min = value;
        break;
    }
  }

  [[nodiscard]] unsigned int min() const { return m_min; }
  [[nodiscard]] unsigned int max() const { return m_max; }

 private:
  unsigned int m_min = 0;
  unsigned int m_max = 0;
};

struct Window {
  WindowId id;

  WindowBounds x_bounds;
  WindowBounds y_bounds;

  explicit Window(WindowId window_id) : id{window_id} {}

  [[nodiscard]] bool contains(Vec2<unsigned int> coord) const {
    const auto min = min_bounds();
    const auto max = max_bounds();
    return (coord.x >= min.x && coord.y >= min.y && coord.x < max.x &&
            coord.y < max.y);
  }

  [[nodiscard]] Vec2<unsigned int> min_bounds() const {
    return {x_bounds.min(), y_bounds.min()};
  }

  [[nodiscard]] Vec2<unsigned int> max_bounds() const {
    return {x_bounds.max(), y_bounds.max()};
  }
};

class Gpu {
 public:
  static constexpr u32 ScreenWidth = 240;
  static constexpr u32 ScreenHeight = 160;

  struct PerPixelContext {
    class EnabledLayerMap {
     public:
      [[nodiscard]] bool layer_visible(Dispcnt::BackgroundLayer layer) const {
        return test_bit(m_enabled_layer_bits, static_cast<u32>(layer));
      }

      [[nodiscard]] bool enable_special_effects() const {
        return test_bit(m_enabled_layer_bits, 5);
      }

      void set_enabled_layer_bits(u8 enabled_layer_bits) {
        m_enabled_layer_bits = enabled_layer_bits;
      }

      void reset() { m_enabled_layer_bits = 0xff; }

     private:
      u8 m_enabled_layer_bits = 0xff;
    };

    Gpu* gpu;
    std::array<unsigned int, ScreenWidth> priorities{};
    std::array<Color, ScreenWidth> top_pixels{};
    std::array<int, ScreenWidth> sprite_priorities{};
    std::array<StaticVector<PriorityInfo, 6>, ScreenWidth> pixel_priorities{};
    std::array<Color, ScreenWidth> backdrop_scanline{};
    std::array<EnabledLayerMap, ScreenWidth> window_layers_enabled{};
    unsigned int scanline = 0;

    void put_pixel(unsigned int x,
                   Color color,
                   Dispcnt::BackgroundLayer layer,
                   unsigned int priority);

    void put_sprite_pixel(unsigned int x, Color color, unsigned int priority);
  };

  class AffineScrollProxy {
   public:
    // Internal affine scroll registers should be updated when the real register
    // is written.
    AffineScrollProxy(int& reg, int& internal_reg)
        : m_register{&reg}, m_internal_register{&internal_reg} {}

    void write_byte(unsigned int byte, u8 value) {
      *m_register = gb::write_byte(*m_register, byte, value);
    }

    void on_after_write() const noexcept { *m_internal_register = *m_register; }

    [[nodiscard]] std::size_t size_bytes() const { return 4; }

    [[nodiscard]] nonstd::span<u8> byte_span() const noexcept {
      return nonstd::span<u8>{nullptr,
                              static_cast<nonstd::span<u8>::index_type>(0)};
    }

   private:
    int* m_register;
    int* m_internal_register;
  };

  struct Background {
    Bgcnt control;
    Dispcnt::BackgroundLayer layer;
    Vec2<int> affine_scroll{0, 0};
    Vec2<int> internal_affine_scroll{0, 0};

    Vec2<u16> scroll{0, 0};
    nonstd::span<Color> scanline;

    std::array<s16, 4> affine_matrix{1 << 8, 0, 0, 1 << 8};

    AffineScrollProxy affine_scroll_x_proxy{affine_scroll.x,
                                            internal_affine_scroll.x};
    AffineScrollProxy affine_scroll_y_proxy{affine_scroll.y,
                                            internal_affine_scroll.y};

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

 private:
  std::array<std::array<Color, ScreenWidth>, 4> m_scanlines;

 public:
  Background bg0{*this, Dispcnt::BackgroundLayer::Zero, m_scanlines[0]};
  Background bg1{*this, Dispcnt::BackgroundLayer::One, m_scanlines[1]};
  Background bg2{*this, Dispcnt::BackgroundLayer::Two, m_scanlines[2]};
  Background bg3{*this, Dispcnt::BackgroundLayer::Three, m_scanlines[3]};

  Bldcnt bldcnt{0};
  Bldalpha bldalpha{0};
  Bldy bldy{0};

  Window window0{WindowId::Zero};
  Window window1{WindowId::One};
  WindowIn window_in;
  WindowOut window_out;

  void sort_backgrounds();

  void render_scanline(unsigned int scanline);

  [[nodiscard]] nonstd::span<const Color> framebuffer() const {
    return {m_framebuffer};
  }

 private:
  void render_mode2(Background& background);
  void render_mode3(unsigned int scanline);
  void render_mode4(unsigned int scanline);

  void render_tile_row_4bpp(nonstd::span<Color> framebuffer,
                            nonstd::span<const u8> palette_bank,
                            Vec2<unsigned int> position,
                            unsigned int priority,
                            Gpu::PerPixelContext& per_pixel_context,
                            nonstd::span<const u8> tile_pixels,
                            Dispcnt::BackgroundLayer layer,
                            PriorityInfo priority_info,
                            bool horizontal_flip = false);
  void render_background(Background background, unsigned int scanline);
  void render_sprites(unsigned int scanline);

  nonstd::span<const Color> framebuffer_from_layer(
      Dispcnt::BackgroundLayer layer) const {
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
      case Dispcnt::BackgroundLayer::Backdrop:
        return m_per_pixel_context.backdrop_scanline;
      default:
        throw std::runtime_error("invalid layer");
    }
  }
  StaticVector<Window, 3> m_active_windows;

  std::array<Background*, 4> m_backgrounds{&bg0, &bg1, &bg2, &bg3};
  decltype(m_backgrounds)::iterator m_backgrounds_end = m_backgrounds.begin();

  std::array<Color, ScreenWidth> sprite_scanline;

  nonstd::span<u8> m_vram;
  nonstd::span<u8> m_palette_ram;
  nonstd::span<u8> m_oam_ram;

  PerPixelContext m_per_pixel_context{this};
  std::vector<Color> m_framebuffer;
};
}  // namespace gb::advance
