#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include "cpu.h"
#include "emulator.h"
#include "gpu.h"
#include "input.h"
#include "lcd.h"
#include "memory.h"
#include "registers/cgb.h"
#include "registers/div.h"
#include "registers/palette.h"
#include "registers/sound.h"
#include "renderer.h"
#include "rom_loader.h"
#include "sdl_renderer.h"
#include "sound.h"
#include "timers.h"

namespace gb {
static bool file_exists(const std::string& path) {
  std::ifstream file{path};

  return file.good();
}

static std::vector<u8> load_rom(const std::string& rom_name) {
  std::ifstream rom(rom_name, std::ios::in | std::ios::binary);
  rom.seekg(0, std::ios::end);

  int rom_size = rom.tellg();

  std::cout << "rom size: " << rom_size << std::endl;

  rom.seekg(0);
  std::vector<u8> rom_data(std::istreambuf_iterator<char>{rom},
                           std::istreambuf_iterator<char>{});

  std::cout << std::endl;

  rom.close();

  return rom_data;
}

void run_with_options(const std::string& rom_name, bool trace, bool save) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
    std::cerr << "Error SDL_Init: " << SDL_GetError() << '\n';
    return;
  }
  SDL_JoystickEventState(SDL_ENABLE);

  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_JoystickOpen(i) == nullptr) {
      std::cerr << "JoypadOpen: " << SDL_GetError() << '\n';
    }
  }

  std::cout << rom_name << std::endl;
  std::string save_ram_path = rom_name + ".sav";

  auto file_flags = std::fstream::out | std::fstream::in |
                    std::fstream::binary | std::fstream::ate;

  if (!file_exists(save_ram_path)) {
    file_flags |= std::fstream::trunc;
  }

  std::vector<u8> rom_data = load_rom(rom_name);
  const RomHeader rom_header = gb::parse_rom(rom_data);

  gb::Memory memory{rom_header.mbc};

  gb::HdmaTransfer hdma{memory};

  memory.reset();

  memory.load_rom(std::move(rom_data));

  if (save) {
    // Workaround for std::fstream being move only
    std::shared_ptr<std::fstream> save_ram_file =
        std::make_shared<std::fstream>(save_ram_path, file_flags);
    std::cout << strerror(errno) << std::endl;
    if (!save_ram_file->good()) {
      throw std::runtime_error("could not open save ram file");
    }

    if (save_ram_file->tellg() == 0) {
      std::fill_n(std::ostreambuf_iterator<char>{*save_ram_file},
                  rom_header.save_ram_size, 0xff);
      std::vector<u8> save_ram_data(rom_header.save_ram_size, 0xff);
      memory.load_save_ram(std::move(save_ram_data));
    } else {
      save_ram_file->seekg(0);
      std::vector<u8> save_ram(std::istreambuf_iterator<char>{*save_ram_file},
                               std::istreambuf_iterator<char>{});
      memory.load_save_ram(std::move(save_ram));
    }
    save_ram_file->flush();

    memory.add_save_ram_write_listener(
        [file = std::move(save_ram_file)](std::size_t index, u8 val) {
          file->seekp(index);
          file->put(val);
        });
  } else {
    std::vector<u8> save_ram_data(rom_header.save_ram_size, 0xff);
    memory.load_save_ram(std::move(save_ram_data));
  }

  gb::Timers timers;

  gb::Cpu cpu{memory};

  cpu.set_debug(trace);

#if __linux__ && !defined RASPBERRYPI
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

#ifdef __SWITCH__
  constexpr int WINDOW_WIDTH = 1920;
  constexpr int WINDOW_HEIGHT = 1080;
#else
  constexpr int WINDOW_WIDTH = 160 * 2;
  constexpr int WINDOW_HEIGHT = 144 * 2;
#endif

  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  SDL_Window* window = SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH,
                                        WINDOW_HEIGHT, SDL_WINDOW_FULLSCREEN);

  if (!window) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  auto delete_renderer = [](SDL_Renderer* r) { SDL_DestroyRenderer(r); };

  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
      sdl_renderer{
          SDL_CreateRenderer(
              window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
          delete_renderer};

  if (!sdl_renderer) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  SDL_RenderSetLogicalSize(sdl_renderer.get(), 160, 144);

  std::cerr << "Init Audio SDL Error: " << SDL_GetError() << '\n';

  SDL_AudioSpec want, have;
  std::memset(&want, 0, sizeof(want));

  want.freq = gb::SOUND_SAMPLE_FREQUENCY;
  want.format = AUDIO_F32;
  want.channels = 2;
  want.samples = 4096;
  want.callback = nullptr;

  SDL_AudioDeviceID audio_device =
      SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (audio_device == 0) {
    std::cerr << "OpenAudioDevice SDL Error: " << SDL_GetError() << '\n';
    return;
  }

  SDL_PauseAudioDevice(audio_device, 0);
  std::cerr << "PauseAudio SDL Error: " << SDL_GetError() << '\n';

  gb::SdlRenderer renderer{std::move(sdl_renderer)};

  auto sprite_filter = rom_header.is_cgb
              ? [](SpriteAttribute attribute) {
                  return SpriteAttribute::clear_dmg_palette(attribute);
              }
              : [](SpriteAttribute attribute) {
                  return SpriteAttribute::clear_cgb_flags(attribute);
              };
  gb::Gpu gpu{memory, renderer, sprite_filter};

  if (!rom_header.is_cgb) {
    memory.set_write_listener(
        [](u16 addr, u8 palette, const Hardware& hardware) {
          switch (addr) {
            case gb::Registers::Palette::Background::Address:
              hardware.gpu->compute_background_palette(palette);
              break;
            case gb::Registers::Palette::Obj0::Address:
              hardware.gpu->compute_sprite_palette(0, palette);
              break;
            case gb::Registers::Palette::Obj1::Address:
              hardware.gpu->compute_sprite_palette(1, palette);
              break;
          }
        });
  } else {
    memory.set_write_listener([](u16 addr, u8 color, const Hardware& hardware) {
      static_cast<void>(addr);
      static_cast<void>(color);
      static_cast<void>(hardware);
    });
  }

  gb::Lcd lcd{cpu, gpu};
  gb::Input input;
  gb::Sound sound{memory, audio_device};

  gb::Hardware hardware{&cpu,   &memory, &hdma, &timers,
                        &sound, &input,  &lcd,  &gpu};
  memory.set_hardware(hardware);
// TODO
#if 0
  constexpr int step_ms = 1000 / 60;
  constexpr double FREQUENCY{4194304.0};
  constexpr double SCREEN_REFRESH_TICKS{70224.0};
  constexpr double VSYNC{FREQUENCY / SCREEN_REFRESH_TICKS};

  constexpr double TARGET_SPEED{1000.0 / VSYNC};

  unsigned int total_ticks = 0;
  Uint32 prev_time = 0;
  Uint32 delay = 0;
  double speed_compensation = 0;
#endif

  while (true) {
    // prev_time = SDL_GetTicks();

    SDL_Event e;
    if (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        break;
      } else if (e.type == SDL_KEYUP || e.type == SDL_KEYDOWN) {
        const bool set_key = e.type == SDL_KEYDOWN;
        switch (e.key.keysym.sym) {
          case SDLK_UP:
            input.set_up(set_key);
            break;
          case SDLK_DOWN:
            input.set_down(set_key);
            break;
          case SDLK_LEFT:
            input.set_left(set_key);
            break;
          case SDLK_RIGHT:
            input.set_right(set_key);
            break;
          case SDLK_z:
            input.set_b(set_key);
            break;
          case SDLK_x:
            input.set_a(set_key);
            break;
          case SDLK_RETURN:
            input.set_start(set_key);
            break;
          case SDLK_RSHIFT:
            input.set_select(set_key);
            break;
          default:
            break;
        }
      } else if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
        const bool set_key = e.type == SDL_JOYBUTTONDOWN;

        switch (e.jbutton.button) {
          case 0:
            input.set_a(set_key);
            break;
          case 1:
            input.set_b(set_key);
            break;
          case 10:
            input.set_start(set_key);
            break;
          case 11:
            input.set_select(set_key);
            break;
          case 12:
            input.set_left(set_key);
            break;
          case 13:
            input.set_up(set_key);
            break;
          case 14:
            input.set_right(set_key);
            break;
          case 15:
            input.set_down(set_key);
            break;
          default:
            break;
        }
      }
    }

    bool draw_frame = false;

    while (!draw_frame) {
      const Ticks instruction_ticks = cpu.fetch_and_decode();
      const Ticks interrupt_ticks = cpu.handle_interrupts();

      const auto [ticks, double_ticks] = instruction_ticks + interrupt_ticks;

      if (trace && !cpu.is_halted()) {
        cpu.debug_write();
      }

      memory.update(ticks);

      const auto [render, next_mode] = lcd.update(ticks);
      draw_frame = render;

      visit_optional(next_mode, [&hdma, &cpu](const Lcd::Mode mode) {
        if (hdma.active() && mode == gb::Lcd::Mode::HBlank &&
            !cpu.is_halted()) {
          hdma.transfer_bytes(16);
        }
      });

      bool request_interrupt = input.update();
      if (request_interrupt) {
        cpu.request_interrupt(gb::Cpu::Interrupt::Joypad);
      }
      request_interrupt = timers.update(double_ticks);

      if (request_interrupt) {
        cpu.request_interrupt(gb::Cpu::Interrupt::Timer);
      }

      sound.update(ticks);
    }

    renderer.clear();
    renderer.present();

#if 0
    Uint32 now = SDL_GetTicks();
    speed_compensation += TARGET_SPEED - (now - prev_time);
    delay = (Uint32)speed_compensation;
    speed_compensation -= delay;

    if (delay > 0) {
      Uint32 delay_ticks = SDL_GetTicks();
      Uint32 after_delay_ticks;

      // std::cout << delay << std::endl;
      // SDL_Delay(delay);
      after_delay_ticks = SDL_GetTicks();
      speed_compensation +=
          static_cast<double>(delay) - (after_delay_ticks - delay_ticks);
    }
#endif
    // Uint32 now = SDL_GetTicks();
    // Uint32 diff = now - prev_time;
    // prev_time = now;
    // std::cout << diff << std::endl;
    // if (diff < step_ms) {
    // SDL_Delay(step_ms - diff);
  }
  std::cout << "exit" << std::endl;
}  // namespace gb
}  // namespace gb
