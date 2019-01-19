#include <SDL2/SDL.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "sdl_renderer.h"

int main(int argc, const char** argv) {
  (void)argc;

  static_assert(std::is_same<next_largest_type<u16>::type, u32>::value,
                " not same");
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
  gb::Lcd lcd{cpu, memory};

  SDL_Init(SDL_INIT_VIDEO);

  SDL_Window* window = SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, 160, 144, 0);
  auto delete_renderer = [](SDL_Renderer* r) { SDL_DestroyRenderer(r); };
  std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
      sdl_renderer{
          SDL_CreateRenderer(
              window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
          delete_renderer};
  if (!sdl_renderer.get()) {
    std::cout << "SDL Error: " << SDL_GetError() << std::endl;
  }

  auto renderer{std::make_unique<gb::SdlRenderer>(std::move(sdl_renderer))};

  gb::Gpu gpu{memory, std::move(renderer)};

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
  memory.memory[0xFFFF] = 0x00;
  // cpu.pc = 0x25d;

  cpu.debug_write();
  auto prevTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
  while (true) {
    SDL_Event e;
    if (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        break;
      }
    }
    // std::cin.get();
    int ticks = cpu.fetch_and_decode();
    cpu.debug_write();
    cpu.handle_interrupts();
    lcd.update(ticks);
    auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

    if (time - prevTime > 16) {
      gpu.render();
      prevTime = time;
    }
  }

  return 0;
}
