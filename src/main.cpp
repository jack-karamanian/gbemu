#include <SDL2/SDL.h>
#include <boost/program_options.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include "cpu.h"
#include "gpu.h"
#include "input.h"
#include "lcd.h"
#include "memory.h"
#include "sdl_renderer.h"

namespace po = boost::program_options;

int main(int argc, const char** argv) {
  po::options_description option_desc{"Options"};

  option_desc.add_options()(
      "trace", po::value<bool>()->default_value(false)->implicit_value(true),
      "enables CPU tracing");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, option_desc), vm);

  po::notify(vm);
  const bool trace = vm["trace"].as<bool>();

  gb::Memory memory;
  std::string rom_name = argv[1];

  std::cout << rom_name << std::endl;

  std::ifstream rom(rom_name, std::ios::in | std::ios::binary);
  rom.seekg(0, std::ios::end);

  int rom_size = rom.tellg();

  rom.seekg(0);

  rom.read(reinterpret_cast<char*>(&memory.memory[0]), rom_size);

  rom.close();

  gb::Cpu cpu{memory};

  SDL_Init(SDL_INIT_VIDEO);

  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
  SDL_Window* window = SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, 160, 144, 0);
  auto delete_renderer = [](SDL_Renderer* r) {
    std::cout << "DESTROY SDL RENDERER" << std::endl;
    SDL_DestroyRenderer(r);
  };
  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
      sdl_renderer{
          SDL_CreateRenderer(
              window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
          delete_renderer};
  if (!sdl_renderer.get()) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  std::unique_ptr<gb::SdlRenderer> renderer =
      std::make_unique<gb::SdlRenderer>(std::move(sdl_renderer));
  gb::Gpu gpu{memory, std::move(renderer)};
  gb::Lcd lcd{cpu, memory, gpu};
  gb::Input input{memory};

  cpu.pc = 0x100;
  memory.memory[0xFF05] = 0x00;
  memory.memory[0xFF06] = 0x00;
  memory.memory[0xFF07] = 0x00;
  memory.memory[0xFF10] = 0x80;
  memory.memory[0xFF11] = 0xBF;
  memory.memory[0xFF12] = 0xF3;
  memory.memory[0xFF14] = 0xBF;
  memory.memory[0xFF16] = 0x3F;
  memory.memory[0xFF17] = 0x00;
  memory.memory[0xFF19] = 0xBF;
  memory.memory[0xFF1A] = 0x7F;
  memory.memory[0xFF1B] = 0xFF;
  memory.memory[0xFF1C] = 0x9F;
  memory.memory[0xFF1E] = 0xBF;
  memory.memory[0xFF20] = 0xFF;
  memory.memory[0xFF21] = 0x00;
  memory.memory[0xFF22] = 0x00;
  memory.memory[0xFF23] = 0xBF;
  memory.memory[0xFF24] = 0x77;
  memory.memory[0xFF25] = 0xF3;
  memory.memory[0xFF26] = 0xF1;
  memory.memory[0xFF40] = 0x91;
  memory.memory[0xFF42] = 0x00;
  memory.memory[0xFF43] = 0x00;
  memory.memory[0xFF45] = 0x00;
  memory.memory[0xFF47] = 0xFC;
  memory.memory[0xFF48] = 0xFF;
  memory.memory[0xFF49] = 0xFF;
  memory.memory[0xFF4A] = 0x00;
  memory.memory[0xFF4B] = 0x00;
  memory.memory[0xFFFE] = 0x00;

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

    memory.set(0xff04, rand());
    const bool request_interrupt = input.update();
    if (request_interrupt) {
      cpu.request_interrupt(gb::Cpu::Interrupt::Joypad);
    }

    int ticks = cpu.fetch_and_decode();
    if (!cpu.halted) {
      if (trace) {
        cpu.debug_write();
      }
    }
    ticks += cpu.handle_interrupts();
    lcd.update(ticks);
  }

  return 0;
}
