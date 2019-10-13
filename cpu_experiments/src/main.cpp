#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <fmt/ostream.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/init/Init.h>
#include <range/v3/all.hpp>
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
#include "color.h"
#include "gba/assembler.h"
#include "gba/lcd.h"
#include "gba/input.h"
#include "gba/timer.h"
#include "gba/emulator.h"
#include "gba/dma.h"
#include "gba/gpu.h"
#include "debugger/disassembly_view.h"
#include "debugger/hardware_thread.h"
#include "imgui_memory_editor.h"

namespace gb::advance {

static const char* bool_to_string(bool value) {
  return value ? "true" : "false";
}

class NumberInput {
 public:
  explicit NumberInput(const char* name)
      : m_name{name}, m_button_id{std::string{name} + "-button"} {};

  template <typename Func>
  void render(Func callback) {
    ImGui::InputText(m_name, &m_value);
    ImGui::SameLine();
    ImGui::PushID(m_button_id.c_str());
    if (ImGui::Button("Set")) {
      try {
        const u32 number_value = std::stoi(m_value, nullptr, 16);
        callback(number_value);
      } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
      }
    }
    ImGui::PopID();
  }

 private:
  const char* m_name;
  std::string m_value;
  std::string m_button_id;
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
  const auto [control, _, scroll] = background;
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
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << SDL_GetError() << '\n';
    return;
  }
  folly::CPUThreadPoolExecutor executor{std::thread::hardware_concurrency()};

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

  auto bytes = experiments::assemble("mov r5, #12");

  Mmu mmu;

  // mmu.load_rom(load_file("/home/jack/Downloads/CPUTest.gba"));
  mmu.load_rom(load_file(rom_path));

  DisassemblyInfo disassembly;
  DisassemblyInfo thumb_disassembly;
  DisassemblyInfo iwram_disassembly;

  const auto make_handler_for = [](DisassemblyInfo& disassembly_info) {
    return [&disassembly_info](auto&& res) {
      auto& addr_to_index = disassembly_info.addr_to_index;

      std::transform(res.begin(), res.end(),
                     std::inserter(addr_to_index, addr_to_index.begin()),
                     [i = 0](auto& entry) mutable -> std::pair<u32, u32> {
                       return {entry.loc, i++};
                     });
      disassembly_info.disassembly = std::move(res);
    };
  };

  folly::via(&executor, [&mmu]() {
    return experiments::disassemble(mmu.rom());
  }).thenValue(make_handler_for(disassembly));

  folly::via(&executor, [&mmu]() {
    auto res = experiments::disassemble(mmu.rom(), "thumb");
#if 1
    std::ofstream file{"disasm.asm"};

    for (auto& entry : res) {
      fmt::print(file, "{} {}\n", entry.loc, entry.text.c_str());
    }
#endif
    return res;
  }).thenValue(make_handler_for(thumb_disassembly));

#if 0
  {
    nonstd::span rom_span = mmu.rom();
    const auto num_threads = std::thread::hardware_concurrency();
    const auto slice_size = rom_span.size() / num_threads;

    std::vector<folly::Future<std::vector<experiments::DisassemblyEntry>>>
        futures;

    fmt::print("{} {}\n", num_threads, rom_span.size());

    auto range = ranges::view::ints(0u, num_threads);
    std::transform(
        range.begin(), range.end(), std::back_inserter(futures),
        [slice_size, rom_span, &executor](unsigned int i) {
          auto fut = folly::via(&executor, [rom_span, i, slice_size]() {
            const u32 offset_begin = i * slice_size;

            auto subspan = rom_span.subspan(offset_begin, slice_size);
            auto entries = experiments::disassemble(subspan, "thumb");

            for (auto& entry : entries) {
              entry.loc += offset_begin;
            }
            return entries;
          });
          return fut;
        });

    folly::collectAll(futures)
        .then(&executor,
              [rom_span](auto&& final_futures) {
                std::vector<experiments::DisassemblyEntry> all_entries;
                all_entries.reserve(rom_span.size());
                fmt::print("{} {}\n", final_futures.size(),
                           std::this_thread::get_id());
                for (auto& entries_try : final_futures) {
                  auto& entries = entries_try.value();
                  std::move(entries.begin(), entries.end(),
                            std::back_inserter(all_entries));
                }
                return all_entries;
              })
        .thenValue(make_handler_for(thumb_disassembly));
  }
#endif

  Cpu cpu{mmu};
  ProgramStatus program_status = cpu.program_status();
  program_status.set_irq_enabled(true);
  cpu.set_program_status(program_status);

  Gpu gpu{mmu};
  Dmas dmas{mmu, cpu};

  Lcd lcd{cpu, mmu, dmas, gpu};
  Input input;

  Timers timers{cpu};

  Hardware hardware{&cpu, &lcd, &input, &mmu, &timers, &dmas, &gpu};

  mmu.hardware = hardware;

  cpu.set_reg(Register::R15, 0x08000000);
  // cpu.set_reg(Register::R13, gb::advance::Mmu::IWramEnd - 0x100);

  HardwareThread hardware_thread{hardware};

  bool running = true;

  std::string assembler_string = "";

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
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&event);

      switch (event.type) {
        case SDL_QUIT:
          running = false;
          hardware_thread.signal_vsync();
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
        using namespace std::literals;
        auto reg = static_cast<Register>(i);
        auto value = cpu.reg(reg);
        ImGui::LabelText(("R"s + std::to_string(i)).c_str(), "%d / %08x", value,
                         value);
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
        static NumberInput breakpoint_input{"Breakpoint"};
        breakpoint_input.render([&](u32 value) {
          hardware_thread.push_event(SetBreakpoint{value});
        });
      }
      {
        static NumberInput watchpoint_input{"Watchpoint"};
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
      ImGui::Begin("Editor");

      ImGui::InputTextMultiline("Assembly", &assembler_string, ImVec2{400, 900},
                                ImGuiInputTextFlags_AllowTabInput);

      if (ImGui::Button("Assemble")) {
#if 0
        write_stored_data(assembler_string);
        bytes = experiments::assemble(assembler_string);
        if (!bytes.empty()) {
          cpu = Cpu{mmu};
          cpu.execute();
        }
#endif
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
      static DisassemblyView arm_disassembly_view{"ARM Disassembly", 4};
      static DisassemblyView thumb_disassembly_view{"Thumb Disassembly", 2};
      static DisassemblyView iwram_disassembly_view{"IWRam Disassembly", 4};
      arm_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, cpu,
                                  disassembly);
      thumb_disassembly_view.render(gb::advance::Mmu::RomRegion0Begin, cpu,
                                    thumb_disassembly);
      iwram_disassembly_view.render(gb::advance::Mmu::IWramBegin, cpu,
                                    iwram_disassembly);
    }
    {
      ImGui::Begin("VRAM");
      static MemoryEditor vram_memory_editor;
      auto vram = mmu.vram();
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
        folly::via(&executor, [&mmu]() {
          return experiments::disassemble(mmu.iwram(), arches[arch_index]);
        }).thenValue(make_handler_for(iwram_disassembly));
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
      ImGui::LabelText("Bldcnt", "%04x", gpu.bldcnt);
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
#if 0
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
#endif

      glBindTexture(GL_TEXTURE_2D, texture);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   reinterpret_cast<const void*>(gpu.framebuffer().data()));
      auto error = glGetError();

      if (error != GL_NO_ERROR) {
        std::cerr << "gl error \n";
        std::cerr << glewGetErrorString(error);
      }
      ImGui::Begin("Screen");
      ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texture)),
                   ImVec2{240 * 2, 160 * 2});
      ImGui::End();
    }
    ImGui::Render();
    SDL_GL_MakeCurrent(window, gl_context);

    glClearColor(0, 0, 0, 255);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
    hardware_thread.signal_vsync();
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
  // context.setOption("no-run", 1);
  context.applyCommandLine(argc, argv);

  context.setOption("no-breaks", true);

  int res = context.run();

  if (context.shouldExit()) {
    return res;
  }
  gb::advance::run_emulator_and_debugger(argv[1]);
}
