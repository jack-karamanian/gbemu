#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <string>
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
#include "color.h"
#include "assembler.h"
#include "imgui_memory_editor.h"

static const char* FILE_NAME = "program.s";

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

int main(int argc, char** argv) {
  doctest::Context context;

  context.setOption("abort-after", 5);
  context.applyCommandLine(argc, argv);

  context.setOption("no-breaks", true);

  int res = context.run();

  if (context.shouldExit()) {
    return res;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << SDL_GetError() << '\n';
    return 1;
  }

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

  gb::advance::Mmu mmu;

  mmu.rom = load_file("/home/jack/Downloads/armwrestler.gba");

  auto disassembly = experiments::disassemble(mmu.rom);
  gb::advance::Cpu cpu{mmu};

  cpu.set_reg(gb::advance::Register::R0, 10);
  cpu.set_reg(gb::advance::Register::R15, 0x080002f0);
  cpu.set_reg(gb::advance::Register::R13, gb::advance::Mmu::IWramEnd - 0x100);
  // cpu.set_reg(gb::advance::Register::R15, 0x08000000);

  bool running = true;
  bool execute = false;

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
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);

      switch (event.type) {
        case SDL_QUIT:
          running = false;
          break;
      }
    }

    if (execute) {
      try {
        cpu.execute();
      } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        execute = false;
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
        auto reg = static_cast<gb::advance::Register>(i);
        ImGui::LabelText((reg_string + std::to_string(i)).c_str(), "%d / %08x",
                         cpu.reg(reg));
      }
      ImGui::LabelText("Zero", "%s",
                       cpu.program_status().zero() ? "true" : "false");

      ImGui::Checkbox("Execute", &execute);

      if (ImGui::Button("Step")) {
        try {
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
          cpu = gb::advance::Cpu{mmu};
          execute = true;
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
      ImGui::Begin("Disassembly");
      const gb::u32 offset = (cpu.reg(gb::advance::Register::R15) - 4 -
                              gb::advance::Mmu::RomBegin);
      for (const auto& line : disassembly) {
        const std::string& text = std::get<0>(line);
        const gb::u32 loc = std::get<1>(line);
        if (loc == offset) {
          ImGui::Text("-> %08x %s", loc + gb::advance::Mmu::RomBegin,
                      text.c_str());
          ImGui::SetScrollHereY();
        } else {
          ImGui::Text("%08x %s", loc + gb::advance::Mmu::RomBegin,
                      text.c_str());
        }
      }
      ImGui::End();
    }
    {
      ImGui::Begin("VRAM");
      static MemoryEditor vram_memory_editor;
      vram_memory_editor.DrawContents(mmu.vram.data(), mmu.vram.size());
      ImGui::End();
    }
    {
      nonstd::span<gb::u16> pixels{
          reinterpret_cast<gb::u16*>(mmu.vram.data()),
          static_cast<long>(mmu.vram.size() / sizeof(gb::u16))};

      for (int i = 0; i < 240 * 160; ++i) {
        gb::u16 color = pixels[i];
        framebuffer[i] = {
            static_cast<gb::u8>(gb::convert_space<32, 255>(color & 0x1f)),
            static_cast<gb::u8>(
                gb::convert_space<32, 255>((color >> 5) & 0x1f)),
            static_cast<gb::u8>(
                gb::convert_space<32, 255>((color >> 10) & 0x1f)),
            255};
      }
      framebuffer[344] = {255, 255, 255, 255};
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

  return 0;
}
