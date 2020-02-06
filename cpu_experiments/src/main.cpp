#include <SDL2/SDL.h>
#include <SDL_audio.h>
#include <fmt/ostream.h>
#include <glad/glad.h>
#include <charconv>
#include <string>
#include <vector>
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
// clang-format off
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui/examples/imgui_impl_sdl.h"
#include "imgui/examples/imgui_impl_opengl3.h"
// clang-format on
#include "gba/cpu.h"
#include "color.h"
#include "gba/assembler.h"
#include "gba/lcd.h"
#include "gba/input.h"
#include "gba/timer.h"
#include "gba/emulator.h"
#include "gba/dma.h"
#include "gba/gpu.h"
#include "gba/sound.h"
#include "debugger/disassembly_view.h"
#include "debugger/hardware_thread.h"
#include "imgui_memory_editor.h"

namespace gb::advance {

static constexpr char const* bool_to_string(bool value) {
  return value ? "true" : "false";
}

class NumberInput {
 public:
  constexpr explicit NumberInput(const char* name, int id = 0)

      : m_name{name}, m_button_id{id} {};

  template <typename Func>

  void render(Func callback) {
    ImGui::InputText(m_name, m_value.data(), m_value.size());

    ImGui::SameLine();

    ImGui::PushID(m_button_id);

    if (ImGui::Button("Set")) {
      try {
        u32 number_value = 0;

        auto res = std::from_chars(m_value.data(),
                                   m_value.data() + std::strlen(m_value.data()),
                                   number_value, 16);

        if (res.ec == std::errc{}) {
          callback(number_value);
        } else {
          fmt::print("Invalid string\n");
        }

      } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
      }
    }

    ImGui::PopID();
  }

 private:
  const char* m_name;
  int m_button_id;
  std::array<char, 9> m_value{};
};

static void DispntDisplay(Dispcnt dispcnt) {
  ImGui::LabelText("Mode", "%d", static_cast<u32>(dispcnt.bg_mode()));
  const char* vram_mapping =
      dispcnt.obj_vram_mapping() == Dispcnt::ObjVramMapping::TwoDimensional
          ? "Two Dimensional"
          : "One Dimensional";
  ImGui::LabelText("OBJ VRAM Mapping", "%s", vram_mapping);
  ImGui::LabelText(
      "BG0 Enabled", "%s",
      bool_to_string(dispcnt.layer_enabled(Dispcnt::BackgroundLayer::Zero)));
  ImGui::LabelText(
      "BG1 Enabled", "%s",
      bool_to_string(dispcnt.layer_enabled(Dispcnt::BackgroundLayer::One)));
  ImGui::LabelText(
      "BG2 Enabled", "%s",
      bool_to_string(dispcnt.layer_enabled(Dispcnt::BackgroundLayer::Two)));
  ImGui::LabelText(
      "BG3 Enabled", "%s",
      bool_to_string(dispcnt.layer_enabled(Dispcnt::BackgroundLayer::Three)));
}

static void BackgroundDisplay(Gpu::Background background) {
  const auto [control, _, scroll, scanline] = background;
  ImGui::LabelText("Bits per Pixel", "%d", control.bits_per_pixel());
  ImGui::LabelText("Tile Map Base Block", "%08x", control.tilemap_base_block());
  ImGui::LabelText("Character Base Block", "%08x",
                   control.character_base_block());
  ImGui::LabelText("Priority", "%d", control.priority());
  ImGui::LabelText("Overflow", "%s", [background] {
    switch (background.control.display_overflow()) {
      case Bgcnt::DisplayOverflow::Transparent:
        return "Transparent";
      case Bgcnt::DisplayOverflow::Wraparound:
        return "Wraparound";
    }
  }());
  const auto screen_size = control.screen_size().screen_size;
  ImGui::LabelText("Tile Map Width", "%d", screen_size.width);
  ImGui::LabelText("Tile Map Height", "%d", screen_size.height);
  ImGui::LabelText("X", "%d", scroll.x);
  ImGui::LabelText("Y", "%d", scroll.y);
}

static std::vector<gb::u8> load_file(const std::string_view file_name) {
  std::ifstream file{file_name.data(), std::ios::in | std::ios::binary};

  std::vector<gb::u8> data(std::istreambuf_iterator<char>{file},
                           std::istreambuf_iterator<char>{});

  return data;
}

void run_emulator_and_debugger(std::string_view rom_path) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    std::cerr << SDL_GetError() << '\n';
    return;
  }

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

  SDL_AudioSpec want;
  SDL_AudioSpec have;

  memset(&want, 0, sizeof(want));
  want.freq = 44100;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = nullptr;

  SDL_AudioDeviceID audio_device;
  if ((audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0)) == 0) {
    fmt::print(std::cerr, "Failed to open audio {}\n", SDL_GetError());
    return;
  }

  SDL_PauseAudioDevice(audio_device, 0);

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);

  gladLoadGL();

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init("#version 410 core");

  Mmu mmu;

  mmu.load_rom(load_file(rom_path));

  DisassemblyInfo arm_disassembly;
  DisassemblyInfo thumb_disassembly;
  DisassemblyInfo iwram_disassembly;

  Cpu cpu{mmu};
  ProgramStatus program_status = cpu.program_status();
  program_status.set_irq_enabled(true);
  cpu.set_program_status(program_status);

  Gpu gpu{mmu};
  Dmas dmas{mmu, cpu};

  Lcd lcd{cpu, mmu, dmas, gpu};
  Input input;

  auto sample_callback =
      [audio_device](nonstd::span<Sound::SampleType> samples) {
        if (SDL_QueueAudio(audio_device, samples.data(),
                           sizeof(Sound::SampleType) * samples.size()) < 0) {
          fmt::print(std::cerr, "{}\n", SDL_GetError());
        }
      };

  Sound sound{sample_callback, dmas};

  Timers timers{cpu, sound};

  Hardware hardware{&cpu, &lcd, &input, &mmu, &timers, &dmas, &gpu, &sound};

  mmu.hardware = hardware;

  cpu.set_reg(Register::R15, 0x08000000);
  cpu.set_reg(Register::R13, 0x03007f00);

  HardwareThread hardware_thread{hardware};

  bool running = true;

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA,
               GL_UNSIGNED_BYTE,
               static_cast<const void*>(gpu.framebuffer().data()));

  while (running) {
    {
      SDL_Event event;
      while (SDL_PollEvent(&event) != 0) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
          case SDL_QUIT:
            running = false;
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
        ImGui::Begin("Frame Time");
        ImGui::LabelText("Frame Rate", "%f", hardware_thread.framerate);
        ImGui::LabelText("Frame Time", "%f", hardware_thread.frametime);
        ImGui::End();
      }

      {
        ImGui::Begin("Cpu");
        ImGui::Checkbox("Execute", &hardware_thread.execute);

        for (gb::u32 i = 0; i < 16; ++i) {
          using namespace std::literals;
          auto reg = static_cast<Register>(i);
          auto value = cpu.reg(reg);
          ImGui::LabelText(("R"s + std::to_string(i)).c_str(), "%d / %08x",
                           value, value);
        }

        ImGui::LabelText("Negative", "%s",
                         bool_to_string(cpu.program_status().negative()));
        ImGui::LabelText("Zero", "%s",
                         bool_to_string(cpu.program_status().zero()));
        ImGui::LabelText("Overflow", "%s",
                         bool_to_string(cpu.program_status().overflow()));
        ImGui::LabelText("Carry", "%s",
                         bool_to_string(cpu.program_status().carry()));
        ImGui::LabelText("Thumb", "%s",
                         bool_to_string(cpu.program_status().thumb_mode()));

        ImGui::LabelText("Interrupts Enabled", "%04x",
                         cpu.interrupts_enabled.data());
        ImGui::LabelText("Interrupts Requested", "%04x",
                         cpu.interrupts_requested.data());
        if (ImGui::Button("Execute")) {
          hardware_thread.push_event(SetExecute{true});
        }

        {
          static NumberInput breakpoint_input{"Breakpoint", 0};
          breakpoint_input.render([&](u32 value) {
            hardware_thread.push_event(SetBreakpoint{value});
          });
        }
        {
          static NumberInput watchpoint_input{"Watchpoint", 1};
          watchpoint_input.render([&](u32 value) {
            hardware_thread.push_event(SetWatchpoint{value});
          });
        }

        if (ImGui::Button("Step")) {
          try {
            gb::advance::execute_hardware(hardware);
            // cpu.execute();
          } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
          }
        }
        ImGui::End();
      }

      {
        ImGui::Begin("Memory");

        static MemoryEditor memory_editor;

        memory_editor.Cols = 4;
        ImGui::BeginChild("#rom_bytes");
        auto rom = mmu.rom();
        memory_editor.DrawContents(rom.data(), rom.size());
        ImGui::EndChild();

        ImGui::End();
      }
      {
        static DisassemblyView arm_disassembly_view{"ARM Disassembly"};
        static DisassemblyView thumb_disassembly_view{"Thumb Disassembly"};
        static DisassemblyView iwram_disassembly_view{"IWRam Disassembly"};

        arm_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, 4, cpu,
                                    arm_disassembly);
        thumb_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, 2, cpu,
                                      thumb_disassembly);
#if 0
        if (arm_disassembly_task.ready()) {
          arm_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, cpu,
                                      disassembly);
        }
        if (thumb_disassembly_task.ready()) {
          thumb_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, cpu,
                                        thumb_disassembly);
        }
        iwram_disassembly_view.render(gb::advance::Mmu::IWramBegin, cpu,
                                      iwram_disassembly);
#endif
      }
      {
        ImGui::Begin("VRAM");
        static MemoryEditor vram_memory_editor;
        const auto vram = mmu.vram();
        vram_memory_editor.DrawContents(vram.data(), vram.size());
        ImGui::End();
      }
      {
        using namespace std::literals;
        static constexpr std::array<std::string_view, 2> arches = {"arm"sv,
                                                                   "thumb"sv};
        static int arch_index = 0;

        ImGui::Begin("IWRam");
        ImGui::RadioButton("ARM", &arch_index, 0);
        ImGui::RadioButton("Thumb", &arch_index, 1);

        if (ImGui::Button("Disassemble")) {
        }

        static MemoryEditor memory_editor;
        auto iwram = mmu.iwram();
        memory_editor.DrawContents(iwram.data(), iwram.size());
        ImGui::End();
      }
      {
        ImGui::Begin("EWram");
        static MemoryEditor memory_editor;
        auto ewram = mmu.ewram();
        memory_editor.DrawContents(ewram.data(), ewram.size());
        ImGui::End();
      }
      {
        ImGui::Begin("DMA");

        for (const Dma& dma : dmas.span()) {
          ImGui::Text("DMA %d", static_cast<u32>(dma.number()));
          ImGui::LabelText("Source", "%08x", dma.source);
          ImGui::LabelText("Dest", "%08x", dma.dest);
          ImGui::LabelText("Count", "%08x", dma.count);
          ImGui::LabelText("Control", "%08x", dma.control().data());
        }
        ImGui::End();
      }
      {
        ImGui::Begin("Dispcnt");
        DispntDisplay(gpu.dispcnt);
        ImGui::LabelText("Bldcnt", "%04x", gpu.bldcnt.data());
        ImGui::End();
      }
      {
        ImGui::Begin("Background");
        ImGui::Text("BG0");
        BackgroundDisplay(gpu.bg0);

        ImGui::Text("BG1");
        BackgroundDisplay(gpu.bg1);

        ImGui::Text("BG2");
        BackgroundDisplay(gpu.bg2);

        ImGui::Text("BG3");
        BackgroundDisplay(gpu.bg3);
        ImGui::End();
      }
      {
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     reinterpret_cast<const void*>(gpu.framebuffer().data()));
        auto error = glGetError();

        if (error != GL_NO_ERROR) {
          std::cerr << "gl error \n";
        }
      }
    }
    ImGui::Begin("Screen");
    ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texture)),
                 ImVec2{240 * 2, 160 * 2});
    ImGui::End();
    ImGui::Render();
    SDL_GL_MakeCurrent(window, gl_context);

    glClearColor(0, 0, 0, 255);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
    hardware_thread.run_frame();
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
  doctest::Context context;

  context.setOption("abort-after", 5);
  // context.setOption("no-run", 1);
  context.applyCommandLine(argc, argv);

  context.setOption("no-breaks", true);

  int res = context.run();

  if (context.shouldExit()) {
    return res;
  }
  gb::advance::run_emulator_and_debugger(argv[1]);
  return 0;
}
