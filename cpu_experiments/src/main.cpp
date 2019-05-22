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
#include "assembler.h"
#include "imgui_memory_editor.h"

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

  auto bytes = experiments::assemble("mov r5, #12");
  for (gb::u8 c : bytes) {
    std::cout << std::hex << +c;
  }
  std::cout << '\n';

  gb::advance::Cpu cpu{{bytes}};

  cpu.set_reg(gb::advance::Register::R0, 10);

  bool running = true;
  std::string reg_string{"R"};
  std::string assembler_string{""};
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

    if (cpu.reg(gb::advance::Register::R15) < bytes.size()) {
      cpu.execute();
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);

    ImGui::Begin("Cpu");

    for (gb::u32 i = 0; i < 16; ++i) {
      auto reg = static_cast<gb::advance::Register>(i);
      ImGui::LabelText((reg_string + std::to_string(i)).c_str(), "%d",
                       cpu.reg(reg));
    }
    ImGui::End();

    {
      ImGui::Begin("Editor");

      ImGui::InputTextMultiline("Assembly", &assembler_string,
                                ImVec2{400, 900});

      if (ImGui::Button("Assemble")) {
        bytes = experiments::assemble(assembler_string);
        if (bytes.size() > 0) {
          // TODO: Reset the cpu?
          cpu = gb::advance::Cpu{{bytes}};
          cpu.execute();
        }
      }
      ImGui::End();
    }
    {
      ImGui::Begin("Memory");

      static MemoryEditor memory_editor;

      memory_editor.DrawContents(bytes.data(), bytes.size());

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
