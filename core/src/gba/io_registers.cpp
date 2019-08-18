#include <doctest/doctest.h>
#include <fmt/printf.h>
#include <array>
#include <range/v3/all.hpp>
#include "io_registers.h"
#include "utils.h"

namespace gb::advance {
using namespace gb::advance::hardware;

static constexpr auto gen_table() {
  constexpr std::array<std::pair<u32, u32>, 99> io_register_sizes_ = {{
      {DISPCNT, 2},     {GREENSWAP, 2},   {DISPSTAT, 2},    {VCOUNT, 2},
      {BG0CNT, 2},      {BG1CNT, 2},      {BG2CNT, 2},      {BG3CNT, 2},
      {BG0HOFS, 2},     {BG0VOFS, 2},     {BG1HOFS, 2},     {BG1VOFS, 2},
      {BG2HOFS, 2},     {BG2VOFS, 2},     {BG3HOFS, 2},     {BG3VOFS, 2},
      {BG2PA, 2},       {BG2PB, 2},       {BG2PC, 2},       {BG2PD, 2},
      {BG2X, 4},        {BG2Y, 4},        {BG3PA, 2},       {BG3PB, 2},
      {BG3PC, 2},       {BG3PD, 2},       {BG3X, 4},        {BG3Y, 4},
      {WIN0H, 2},       {WIN1H, 2},       {WIN0V, 2},       {WIN1V, 2},
      {WININ, 2},       {WINOUT, 2},      {MOSAIC, 2},      {BLDCNT, 2},
      {BLDALPHA, 2},    {BLDY, 2},        {SOUND1CNT_L, 2}, {SOUND1CNT_H, 2},
      {SOUND1CNT_X, 2}, {SOUND2CNT_L, 2}, {SOUND2CNT_H, 2}, {SOUND3CNT_L, 2},
      {SOUND3CNT_H, 2}, {SOUND3CNT_X, 2}, {SOUND4CNT_L, 2}, {SOUND4CNT_H, 2},
      {SOUNDCNT_L, 2},  {SOUNDCNT_H, 2},  {SOUNDCNT_X, 2},  {SOUNDBIAS, 2},
      {FIFO_A, 4},      {FIFO_B, 4},      {DMA0SAD, 4},     {DMA0DAD, 4},
      {DMA0CNT_L, 2},   {DMA0CNT_H, 2},   {DMA1SAD, 4},     {DMA1DAD, 4},
      {DMA1CNT_L, 2},   {DMA1CNT_H, 2},   {DMA2SAD, 4},     {DMA2DAD, 4},
      {DMA2CNT_L, 2},   {DMA2CNT_H, 2},   {DMA3SAD, 4},     {DMA3DAD, 4},
      {DMA3CNT_L, 2},   {DMA3CNT_H, 2},   {TM0COUNTER, 2},  {TM0CONTROL, 2},
      {TM1COUNTER, 2},  {TM1CONTROL, 2},  {TM2COUNTER, 2},  {TM2CONTROL, 2},
      {TM3COUNTER, 2},  {TM3CONTROL, 2},  {SIODATA32, 4},   {SIOMULTI0, 2},
      {SIOMULTI1, 2},   {SIOMULTI2, 2},   {SIOMULTI3, 2},   {SIOCNT, 2},
      {SIOMLT_SEND, 2}, {SIODATA8, 2},    {KEYINPUT, 2},    {KEYCNT, 2},
      {RCNT, 2},        {JOYCNT, 2},      {JOY_RECV, 4},    {JOY_TRANS, 4},
      {JOYSTAT, 2},     {IE, 2},          {IF, 2},          {WAITCNT, 2},
      {IME, 2},         {POSTFLG, 1},     {HALTCNT, 1},
  }};

  std::array<u32, 0x410> res{};
  constexpr_fill(res, 0xffffffff);

  constexpr_for_each(io_register_sizes_, [&res](auto pair) {
    const auto [io_addr, size] = pair;
    const auto addr = io_addr & ~0xff000000;
    for (auto i = addr; i < addr + size; ++i) {
      res[i] = io_addr;
    }
  });

  return res;
}

static constexpr auto io_register_sizes = gen_table();

IoRegisterResult select_io_register(u32 addr) {
  const u32 masked_addr = addr & ~0xff000000;
  assert(masked_addr < io_register_sizes.size());

  const auto io_addr = io_register_sizes[masked_addr];

  if (io_addr != 0xffffffff) {
    return {io_addr, addr - io_addr};
  }
  fmt::printf("register %08x not found\n", addr);
  throw std::runtime_error("io register not found");
}

TEST_CASE("select_io_register should find an address") {
  const auto [addr, offset] = select_io_register(DMA0SAD);
  CHECK(addr == DMA0SAD);
  CHECK(offset == 0);
}

TEST_CASE("select_io_register should find an address with an offset") {
  const auto [addr, offset] = select_io_register(DMA0SAD + 1);
  CHECK(addr == DMA0SAD);
  CHECK(offset == 1);
}

}  // namespace gb::advance
