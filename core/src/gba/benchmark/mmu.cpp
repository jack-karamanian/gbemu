#define DOCTEST_CONFIG_IMPLEMENT
#include "gba/mmu.h"
#include <benchmark/benchmark.h>
#include "gba/cpu.h"
#include "gba/dma.h"
#include "gba/emulator.h"
#include "gba/gpu.h"
#include "gba/input.h"
#include "gba/lcd.h"
#include "gba/sound.h"
#include "gba/timer.h"

using namespace gb::advance;
using namespace gb;
static void copy_memory(benchmark::State& state) {
  Mmu mmu;
  for ([[maybe_unused]] auto _ : state) {
    mmu.copy_memory({Mmu::EWramBegin, Mmu::AddrOp::Increment},
                    {Mmu::IWramEnd - 0x100, Mmu::AddrOp::Decrement}, 1000, 4);
  }
}

static std::vector<gb::u8> load_file(const std::string_view file_name) {
  std::ifstream file{file_name.data(), std::ios::in | std::ios::binary};

  std::vector<gb::u8> data(std::istreambuf_iterator<char>{file},
                           std::istreambuf_iterator<char>{});

  return data;
}

struct Emulator {
  Mmu mmu;
  Cpu cpu{mmu};

  Gpu gpu{mmu};
  Dmas dmas{mmu, cpu};

  Lcd lcd{cpu, mmu, dmas, gpu};
  Input input;
  Sound sound{[](auto) {}, dmas};

  Timers timers{cpu, sound};

  Hardware hardware{&cpu, &lcd, &input, &mmu, &timers, &dmas, &gpu, &sound};

  Emulator() { mmu.hardware = hardware; }
};

static std::string g_rom_file;

static void bench_integration(benchmark::State& state) {
  if (g_rom_file.empty()) {
    state.SkipWithError("ROM path not specified");
  }
  using namespace gb::advance;
  Emulator emulator;
  emulator.cpu.set_reg(Register::R15, 0x08000000);
  emulator.mmu.load_rom(load_file(g_rom_file));
  for ([[maybe_unused]] auto _ : state) {
    bool run = false;
    while (!run) {
      benchmark::DoNotOptimize(run = execute_hardware(emulator.hardware));
    }
  }
}

static void bench_at(benchmark::State& state) {
  Emulator emu;

  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000dc));
  }
}

static void bench_at_sequential(benchmark::State& state) {
  Emulator emu;
  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000d4));
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000d8));
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000dc));
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000de));
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000dc));
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000d4));
    benchmark::DoNotOptimize(emu.mmu.at<u16>(0x040000d8));
  }
}

static void bench_set(benchmark::State& state) {
  Emulator emu;
  for ([[maybe_unused]] auto _ : state) {
    emu.mmu.set<u16>(0x04000000, 10);
  }
}

static void bench_set_sequential(benchmark::State& state) {
  Emulator emu;
  for ([[maybe_unused]] auto _ : state) {
    emu.mmu.set<u16>(0x040000d4, 10);
    emu.mmu.set<u16>(0x040000d8, 10);
    emu.mmu.set<u16>(0x040000dc, 10);
    emu.mmu.set<u16>(0x040000de, 10);
    emu.mmu.set<u16>(0x040000dc, 10);
    emu.mmu.set<u16>(0x040000d4, 10);
    emu.mmu.set<u16>(0x040000d8, 10);
  }
}

BENCHMARK(bench_integration)
    ->Repetitions(4)
    ->Unit(benchmark::TimeUnit::kMillisecond);
BENCHMARK(copy_memory);
BENCHMARK(bench_at);
BENCHMARK(bench_at_sequential);
BENCHMARK(bench_set);
BENCHMARK(bench_set_sequential);

int main(int argc, char** argv) {
  std::vector<char*> args(argv, argv + argc);

  auto rom_arg = std::find_if(args.begin(), args.end(), [](const char* arg) {
    return std::strcmp(arg, "--rom-file") == 0;
  });

  if (rom_arg != args.end() && (rom_arg + 1) != args.end()) {
    g_rom_file = *(rom_arg + 1);
    args.erase(rom_arg, rom_arg + 1);
  }

  int arg_count = args.size();
  benchmark::Initialize(&arg_count, args.data());
  benchmark::RunSpecifiedBenchmarks();
}
// BENCHMARK(bench_set_erasure_sequential);
