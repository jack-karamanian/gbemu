#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/init/Init.h>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
// clang-format off
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM <GL/glew.h>
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui/examples/imgui_impl_sdl.h"
#include "imgui/examples/imgui_impl_opengl3.h"
// clang-format on
#include "gba/cpu.h"
#include "memory.h"
#include "color.h"
#include "gba/assembler.h"
#include "gba/lcd.h"
#include "gba/input.h"
#include "debugger/disassembly_view.h"
#include "debugger/hardware_thread.h"
#include "imgui_memory_editor.h"

namespace gb::advance {

static const char* FILE_NAME = "program.s";

static const char* bool_to_string(bool value) {
  return value ? "true" : "false";
}

static std::string read_stored_data() {
  std::ifstream file{FILE_NAME};

  if (!file.good()) {
    return "";
  }

  std::istreambuf_iterator<char> iterator{file};

  return std::string(iterator, std::istreambuf_iterator<char>{});
}

static void write_stored_data(const std::string& assembler) {
  std::ofstream file{FILE_NAME, std::ios::out | std::ios::trunc};

  if (file.good()) {
    file << assembler;
  }
}

static std::vector<gb::u8> load_file(const std::string_view file_name) {
  std::ifstream file{file_name.data(), std::ios::in | std::ios::binary};

  std::vector<gb::u8> data(std::istreambuf_iterator<char>{file},
                           std::istreambuf_iterator<char>{});

  return data;
}

void run_emulator_and_debugger() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << SDL_GetError() << '\n';
    return;
  }
  folly::CPUThreadPoolExecutor executor{8};

  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_Window* window = SDL_CreateWindow(
      "CPU Experiments", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920,
      1080, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);

  glewInit();

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init("#version 410 core");

  std::vector<gb::u8> vram(0x18000, 0);
  std::vector<gb::Color> framebuffer(160 * 240);

  auto bytes = experiments::assemble("mov r5, #12");

  Mmu mmu;

  mmu.rom =
      load_file("/home/jack/Downloads/Super Dodge Ball Advance (USA).gba");

  DisassemblyInfo disassembly;
  DisassemblyInfo thumb_disassembly;

  {
    const auto make_handler_for = [](DisassemblyInfo& disassembly_info) {
      return [&disassembly_info](auto&& res) {
        disassembly_info.disassembly = std::move(res);

        for (int i = 0; i < disassembly_info.disassembly.size(); ++i) {
          const auto& entry = disassembly_info.disassembly[i];
          disassembly_info.addr_to_index[entry.loc] = i;
        }
      };
    };

    folly::via(&executor, [&mmu]() {
      return experiments::disassemble(mmu.rom);
    }).thenValue(make_handler_for(disassembly));

    folly::via(&executor, [&mmu]() {
      return experiments::disassemble(mmu.rom, "thumb");
    }).thenValue(make_handler_for(thumb_disassembly));
  }

  Cpu cpu{mmu};

  Lcd lcd;
  Input input;

  Hardware hardware{&cpu, &lcd, &input, &mmu};

  mmu.hardware = hardware;

  cpu.set_reg(Register::R0, 10);
  cpu.set_reg(Register::R15, 0x08000000);
  cpu.set_reg(Register::R13, gb::advance::Mmu::IWramEnd - 0x100);

  HardwareThread hardware_thread{hardware};

  bool running = true;

  std::string reg_string{"R"};

  std::string assembler_string = read_stored_data();

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, static_cast<void*>(framebuffer.data()));

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&event);

      switch (event.type) {
        case SDL_QUIT:
          running = false;
          hardware_thread.push_event(Quit{});
          break;

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          bool set = event.type == SDL_KEYDOWN;

          switch (event.key.keysym.sym) {
            case SDLK_UP:
              input.set_up(set);
              break;
            case SDLK_DOWN:
              input.set_down(set);
              break;
            case SDLK_LEFT:
              input.set_left(set);
              break;
            case SDLK_RIGHT:
              input.set_right(set);
              break;
            case SDLK_z:
              input.set_b(set);
              break;
            case SDLK_x:
              input.set_a(set);
              break;
            case SDLK_a:
              input.set_l(set);
              break;
            case SDLK_s:
              input.set_r(set);
              break;
            case SDLK_RETURN:
              input.set_start(set);
              break;
            case SDLK_RSHIFT:
              input.set_select(set);
              break;
            default:
              break;
          }

          break;
        }
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);

    {
      ImGui::Begin("Cpu");

      for (gb::u32 i = 0; i < 16; ++i) {
        auto reg = static_cast<Register>(i);
        auto value = cpu.reg(reg);
        ImGui::LabelText((reg_string + std::to_string(i)).c_str(), "%d / %08x",
                         value, value);
      }

      ImGui::LabelText("Negative", "%s",
                       bool_to_string(cpu.program_status().negative()));
      ImGui::LabelText("Zero", "%s",
                       bool_to_string(cpu.program_status().zero()));
      ImGui::LabelText("Overflow", "%s",
                       bool_to_string(cpu.program_status().overflow()));
      ImGui::LabelText("Thumb", "%s",
                       bool_to_string(cpu.program_status().thumb_mode()));

      if (ImGui::Button("Execute")) {
        hardware_thread.push_event(SetExecute{true});
      }

      {
        static std::string breakpoint_string;
        ImGui::InputText("Breakpoint", &breakpoint_string);
        ImGui::SameLine();
        if (ImGui::Button("Set")) {
          try {
            gb::u32 breakpoint_addr = std::stoi(breakpoint_string, nullptr, 16);
            hardware_thread.push_event(SetBreakpoint{breakpoint_addr});
          } catch (std::exception& e) {
            std::cerr << e.what() << '\n';
          }
        }
      }

      if (ImGui::Button("Step")) {
        try {
          // execute_hardware(hardware);
          cpu.execute();
        } catch (const std::exception& e) {
          std::cerr << e.what() << '\n';
        }
      }
      ImGui::End();
    }

    {
      ImGui::Begin("Editor");

      ImGui::InputTextMultiline("Assembly", &assembler_string, ImVec2{400, 900},
                                ImGuiInputTextFlags_AllowTabInput);

      if (ImGui::Button("Assemble")) {
        write_stored_data(assembler_string);
        bytes = experiments::assemble(assembler_string);
        if (!bytes.empty()) {
          cpu = Cpu{mmu};
          cpu.execute();
        }
      }
      ImGui::End();
    }
    {
      ImGui::Begin("Memory");

      static MemoryEditor memory_editor;

      memory_editor.Cols = 4;
      ImGui::BeginChild("#rom_bytes");
      memory_editor.DrawContents(mmu.rom.data(), mmu.rom.size());
      ImGui::EndChild();

      ImGui::End();
    }
    {
      static DisassemblyView arm_disassembly_view{"ARM Disassembly", 4};
      static DisassemblyView thumb_disassembly_view{"Thumb Disassembly", 2};
      arm_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, cpu,
                                  disassembly);
      thumb_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, cpu,
                                    thumb_disassembly);
    }
    {
      ImGui::Begin("VRAM");
      static MemoryEditor vram_memory_editor;
      vram_memory_editor.DrawContents(mmu.vram.data(), mmu.vram.size());
      ImGui::End();
    }
    {
      ImGui::Begin("IWRam");
      static MemoryEditor memory_editor;
      memory_editor.DrawContents(mmu.iwram.data(), mmu.iwram.size());
      ImGui::End();
    }
    {
      nonstd::span<gb::u16> pixels{
          reinterpret_cast<gb::u16*>(mmu.vram.data()),
          static_cast<long>(mmu.vram.size() / sizeof(gb::u16))};

      for (int i = 0; i < 240 * 160; ++i) {
        u8 color_index = mmu.vram[i];
        u16 color = mmu.palette_ram[color_index * 2];

        framebuffer[i] = {
            static_cast<u8>(convert_space<32, 255>(color & 0x1f)),
            static_cast<u8>(convert_space<32, 255>((color >> 5) & 0x1f)),
            static_cast<u8>(convert_space<32, 255>((color >> 10) & 0x1f)), 255};
      }

      glBindTexture(GL_TEXTURE_2D, texture);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, static_cast<void*>(framebuffer.data()));
      auto error = glGetError();

      if (error != GL_NO_ERROR) {
        std::cerr << "gl error \n";
        std::cerr << glewGetErrorString(error);
      }
      ImGui::Begin("Screen");
      ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texture)),
                   ImVec2{240, 160});
      ImGui::End();
    }
    ImGui::Render();
    SDL_GL_MakeCurrent(window, gl_context);

    glClearColor(0, 0, 0, 255);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}
}  // namespace gb::advance

int main(int argc, char** argv) {
  folly::init(&argc, &argv);
  doctest::Context context;

  context.setOption("abort-after", 5);
  context.applyCommandLine(argc, argv);

  context.setOption("no-breaks", true);

  int res = context.run();

  if (context.shouldExit()) {
    return res;
  }
  gb::advance::run_emulator_and_debugger();
}
