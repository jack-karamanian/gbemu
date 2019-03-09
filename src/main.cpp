#include <SDL2/SDL.h>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "cpu.h"
#include "gpu.h"
#include "input.h"
#include "lcd.h"
#include "memory.h"
#include "registers/palette.h"
#include "registers/sound.h"
#include "renderer.h"
#include "sdl_renderer.h"
#include "sound.h"
#include "timers.h"

namespace po = boost::program_options;

static void load_rom(const std::string& rom_name, gb::Memory& memory) {
  std::ifstream rom(rom_name, std::ios::in | std::ios::binary);
  rom.seekg(0, std::ios::end);

  int rom_size = rom.tellg();

  std::cout << "rom size: " << rom_size << std::endl;

  rom.seekg(0);
  std::vector<u8> rom_data(rom_size);

  rom.read(reinterpret_cast<char*>(&rom_data[0]), rom_size);

  std::cout << std::endl;

  rom.close();

  memory.load_rom(rom_data);
}

int main(int argc, const char** argv) {
  po::options_description option_desc{"Options"};

  option_desc.add_options()(
      "trace", po::value<bool>()->default_value(false)->implicit_value(true),
      "enables CPU tracing");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, option_desc), vm);

  po::notify(vm);
  const bool trace = vm["trace"].as<bool>();

  gb::Memory memory{};
  memory.reset();

  std::string rom_name = argv[1];
  std::cout << rom_name << std::endl;

  load_rom(rom_name, memory);

  gb::Timers timers{memory};
  gb::Cpu cpu{memory};

  cpu.set_debug(trace);

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

#if __linux__ && !defined RASPBERRYPI
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

  SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
  SDL_Window* window =
      SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 160, 144, SDL_WINDOW_SHOWN);

  auto delete_renderer = [](SDL_Renderer* r) { SDL_DestroyRenderer(r); };

  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
      sdl_renderer{
          SDL_CreateRenderer(
              window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
          delete_renderer};
  if (!sdl_renderer) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  SDL_AudioSpec want, have;
  std::memset(&want, 0, sizeof(want));

  want.freq = 48000;
  want.format = AUDIO_F32;
  want.channels = 2;
  want.samples = 4096;

  SDL_AudioDeviceID audio_device =
      SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);

  SDL_PauseAudioDevice(audio_device, 0);

  std::shared_ptr<gb::SdlRenderer> renderer =
      std::make_shared<gb::SdlRenderer>(std::move(sdl_renderer));
  gb::Gpu gpu{memory, renderer};

  memory.add_write_listener(
      gb::Registers::Palette::Background::Address,
      [&gpu](u8 palette, u8) { gpu.compute_background_palette(palette); });
  memory.add_write_listener(
      gb::Registers::Palette::Obj0::Address,
      [&gpu](u8 palette, u8) { gpu.compute_sprite_palette(0, palette); });
  memory.add_write_listener(
      gb::Registers::Palette::Obj1::Address,
      [&gpu](u8 palette, u8) { gpu.compute_sprite_palette(1, palette); });

  gb::Lcd lcd{cpu, memory, gpu};
  gb::Input input{memory};
  gb::Sound sound{memory};

  memory.add_write_listener_for_range(
      0xff10, 0xff26,
      [&sound](u16 addr, u8 val, u8) { sound.handle_memory_write(addr, val); });

  sound.set_samples_ready_listener(
      [audio_device](const std::vector<float>& square1_samples) {
        while (SDL_GetQueuedAudioSize(audio_device) > 4096 * sizeof(float)) {
          SDL_Delay(1);
        }
        if (SDL_QueueAudio(audio_device, square1_samples.data(),
                           square1_samples.size() * sizeof(float))) {
          std::cout << "SDL Error: " << SDL_GetError() << std::endl;
        }
      });

  cpu.pc = 0x100;

  while (true) {
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
      if (!cpu.halted) {
        if (trace) {
          cpu.debug_write();
        }
      }

      ticks += cpu.handle_interrupts();
      draw_frame = lcd.update(ticks);

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

    renderer->clear();
    renderer->present();
  }

  return 0;
}
