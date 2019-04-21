#include <SDL2/SDL.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "cpu.h"
#include "emulator.h"
#include "gpu.h"
#include "input.h"
#include "lcd.h"
#include "memory.h"
#include "registers/div.h"
#include "registers/palette.h"
#include "registers/sound.h"
#include "renderer.h"
#include "rom_loader.h"
#include "sdl_renderer.h"
#include "sound.h"
#include "timers.h"

namespace gb {
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
  std::cout << rom_name << std::endl;
  std::filesystem::path save_ram_path =
      std::filesystem::absolute(rom_name + ".sav");

  auto file_flags = std::fstream::out | std::fstream::in |
                    std::fstream::binary | std::fstream::ate;

  if (!std::filesystem::exists(save_ram_path)) {
    file_flags |= std::fstream::trunc;
  }

  std::vector<u8> rom_data = load_rom(rom_name);
  const RomHeader rom_header = gb::parse_rom(rom_data);

  gb::Memory memory{rom_header.mbc};

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

  gb::Timers timers{memory};

  gb::Cpu cpu{memory};

  cpu.set_debug(trace);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

#if __linux__ && !defined RASPBERRYPI
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  SDL_Window* window = SDL_CreateWindow(
      "gbemu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 160 * 2,
      144 * 2, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

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

  SDL_AudioSpec want, have;
  std::memset(&want, 0, sizeof(want));

  want.freq = 48000;
  want.format = AUDIO_F32;
  want.channels = 2;
  want.samples = 4096;

  SDL_AudioDeviceID audio_device =
      SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);

  SDL_PauseAudioDevice(audio_device, 0);

  gb::SdlRenderer renderer{std::move(sdl_renderer)};
  gb::Gpu gpu{memory, renderer};

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
      if (addr == 0xff69) {
        const u8 index = hardware.memory->get_ram(0xff68);
        hardware.gpu->compute_cgb_color(index & 0x3f, color);

        if (gb::test_bit(index, 7)) {
          hardware.memory->set_ram(0xff68, gb::increment_bits(index, 0x3f));
        }
      }
    });
  }

  gb::Lcd lcd{cpu, memory, gpu};
  gb::Input input{memory};
  gb::Sound sound{memory, audio_device};

  gb::Hardware hardware{&cpu, &memory, &timers, &sound, &input, &lcd, &gpu};
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
      }
      if (e.type == SDL_KEYUP || e.type == SDL_KEYDOWN) {
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
      }
    }

    bool draw_frame = false;

    while (!draw_frame) {
      int ticks = cpu.fetch_and_decode();
      if (!cpu.is_halted() && trace) {
        cpu.debug_write();
      }

      ticks += cpu.handle_interrupts();

      draw_frame = lcd.update(ticks);
      /*
      std::optional<LcdState> lcd_state = lcd.update(ticks);
      visit_optional(lcd_state, [&](LcdState state) {
        std::cout << std::hex << +state.interrupts << '\n';
        cpu.request_interrupt(static_cast<Cpu::Interrupt>(state.interrupts));
        draw_frame = state.render;
      });
      */

      bool request_interrupt = input.update();
      if (request_interrupt) {
        cpu.request_interrupt(gb::Cpu::Interrupt::Joypad);
      }
      request_interrupt = timers.update(ticks);

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
}
}  // namespace gb
