#include <fmt/printf.h>
#include <chrono>
#include "gba/cpu.h"
#include "gba/gpu.h"
#include "gba/mmu.h"
#include "timer.h"
#include "utils.h"

#define STUB_ADDR(name)  \
  do {                   \
    static u32 name = 0; \
    return name;         \
  } while (0)

namespace gb::advance {

/*
  .code 32
  stmfd r13!, {r0,r1,r2,r3,r12,r14}
  mov r0, #0x40000000
  add r14, r15, #0
  ldr r15, [r0, #-4]
  ldmfd r13!, {r0, r1, r2, r3, r12, r14}
  subs r15, r14, #4
*/
static std::array<u8, 24> bios_interrupt = {
    0x0f, 0x50, 0x2d, 0xe9, 0x01, 0x03, 0xa0, 0xe3, 0x00, 0xe0, 0x8f, 0xe2,
    0x04, 0xf0, 0x10, 0xe5, 0x0f, 0x50, 0xbd, 0xe8, 0x04, 0xf0, 0x5e, 0xe2};

u32 Mmu::wait_cycles(u32 addr, Cycles cycles) {
  const auto [wait_nonsequential,
              wait_sequential] = [&]() -> std::pair<u32, u32> {
    if (addr >= EWramBegin && addr <= EWramEnd) {
      return {2, 2};
    }

    if (addr >= RomRegion0Begin && addr <= RomRegion0End) {
      return {waitcnt.wait_zero_nonsequential(),
              waitcnt.wait_zero_sequential()};
    }

    if (addr >= RomRegion1Begin && addr <= RomRegion1End) {
      return {waitcnt.wait_one_nonsequential(), waitcnt.wait_one_sequential()};
    }

    if (addr >= RomRegion2Begin) {
      return {waitcnt.wait_two_nonsequential(), waitcnt.wait_two_sequential()};
    }

    return {0, 0};
  }();
  return cycles.internal +
         (cycles.nonsequential +
          (cycles.nonsequential != 0 ? wait_nonsequential : 0)) +
         (cycles.sequential + (cycles.sequential != 0 ? wait_sequential : 0));
}

void Mmu::copy_memory(AddrParam source,
                      AddrParam dest,
                      u32 count,
                      u32 type_size) {
  const auto [source_addr, source_op] = source;
  const auto [dest_addr, dest_op] = dest;

  const int source_stride =
      static_cast<int>(source_op) * static_cast<int>(type_size);
  const int dest_stride =
      static_cast<int>(dest_op) * static_cast<int>(type_size);

  auto [source_storage, resolved_source_addr] =
      select_storage(source_addr, Mmu::DataOperation::Read);
  auto [dest_storage, resolved_dest_addr] =
      select_storage(dest_addr, Mmu::DataOperation::Write);

  for (u32 i = 0; i < count; ++i) {
    for (u32 j = 0; j < type_size; ++j) {
      dest_storage[resolved_dest_addr + j] =
          source_storage[resolved_source_addr + j];
    }
    resolved_source_addr += source_stride;
    resolved_dest_addr += dest_stride;
  }
}

void Mmu::set_bytes(u32 addr, nonstd::span<const u8> bytes) {
  auto [selected_span, resolved_addr] =
      select_storage(addr, DataOperation::Write);
  auto subspan = selected_span.subspan(resolved_addr);

  const bool is_overrunning = bytes.size() > selected_span.size();
  const long difference = bytes.size() - subspan.size();

  const std::size_t copy_size = is_overrunning ? difference : bytes.size();
  // std::copy(converted.begin(), converted.end(), subspan.begin());

  for (std::size_t i = 0; i < copy_size; ++i) {
    subspan[i] = bytes[i];
  }

  if (m_write_handler) {
    m_write_handler(addr, 0);
  }
}

void Mmu::set_hardware_bytes(u32 addr, nonstd::span<const u8> bytes) {
  const auto [io_addr, resolved_addr] = select_io_register(addr);
  auto selected_hardware = select_hardware(io_addr, DataOperation::Write);

  const bool is_overrunning = bytes.size() > selected_hardware.size_bytes();
  const long difference =
      bytes.size() - (selected_hardware.size_bytes() - resolved_addr);

  selected_hardware.write_byte(resolved_addr, bytes);

  if (is_overrunning) {
    set_hardware_bytes(addr + difference, bytes.subspan(difference));
  }

#if 0
  if (m_write_handler) {
    m_write_handler(addr, 0);
  }
#endif
}

std::tuple<nonstd::span<u8>, u32> Mmu::select_storage(u32 addr,
                                                      DataOperation op) {
  if (addr >= IWramBegin && addr <= IWramEnd) {
    return {m_iwram, addr - IWramBegin};
  }

  if (addr >= EWramBegin && addr <= EWramEnd) {
    return {m_ewram, addr - EWramBegin};
  }

  if (addr >= VramBegin && addr <= VramEnd) {
    return {m_vram, addr - VramBegin};
  }

  if (addr >= PaletteBegin && addr <= PaletteEnd) {
    return {m_palette_ram, addr - PaletteBegin};
  }

  if (addr >= OamBegin && addr <= OamEnd) {
    return {m_oam_ram, addr - OamBegin};
  }

  if (addr >= 0x03ffff00 && addr < 0x04000000) {
    const u32 offset = addr & 0xff;
    return {m_iwram, 0x7f00 + offset};
  }

  if (addr >= SramBegin && addr <= SramEnd) {
    return {m_sram, addr - SramBegin};
  }

  for (const auto [begin, end] : rom_regions) {
    if (addr >= begin && addr <= end) {
      return {m_rom, addr - begin};
    }
  }

  // Bios interrupt handler
  if (addr >= 0x00000128 && addr <= 0x0000013c) {
    return {bios_interrupt, addr - 0x128};
  }

  printf("unimplemented select_storage addr %08x\n", addr);
  throw std::runtime_error("unimplemented select storage");
}

IntegerRef Mmu::select_hardware(u32 addr, DataOperation op) {
  switch (addr) {
    case hardware::GREENSWAP:
      STUB_ADDR(greenswap);
    case hardware::DISPCNT:
      return hardware.gpu->dispcnt;
    case hardware::DISPSTAT:
      return hardware.lcd->dispstat;
    case hardware::WAITCNT:
      return waitcnt;
    case hardware::KEYINPUT:
      return *hardware.input;
    case hardware::IME:
      return hardware.cpu->ime;
    case hardware::TM0COUNTER:
      return hardware.timers->timer0.select_counter_register(op);
    case hardware::TM0CONTROL:
      return hardware.timers->timer0.control;
    case hardware::TM1COUNTER:
      return hardware.timers->timer1.select_counter_register(op);
    case hardware::TM1CONTROL:
      return hardware.timers->timer1.control;
    case hardware::TM2COUNTER:
      return hardware.timers->timer2.select_counter_register(op);
    case hardware::TM2CONTROL:
      return hardware.timers->timer2.control;
    case hardware::TM3COUNTER:
      return hardware.timers->timer3.select_counter_register(op);
    case hardware::TM3CONTROL:
      return hardware.timers->timer3.control;
    case hardware::VCOUNT:
      return hardware.lcd->vcount;
    case hardware::BG0CNT:
      return hardware.gpu->bg0.control;
    case hardware::BG1CNT:
      return hardware.gpu->bg1.control;
    case hardware::BG2CNT:
      return hardware.gpu->bg2.control;
    case hardware::BG3CNT:
      return hardware.gpu->bg3.control;
    case hardware::BG0HOFS:
      return hardware.gpu->bg0.scroll.x;
    case hardware::BG0VOFS:
      return hardware.gpu->bg0.scroll.y;
    case hardware::BG1HOFS:
      return hardware.gpu->bg1.scroll.x;
    case hardware::BG1VOFS:
      return hardware.gpu->bg1.scroll.y;
    case hardware::BG2HOFS:
      return hardware.gpu->bg2.scroll.x;
    case hardware::BG2VOFS:
      return hardware.gpu->bg2.scroll.y;
    case hardware::BG3HOFS:
      return hardware.gpu->bg3.scroll.x;
    case hardware::BG3VOFS:
      return hardware.gpu->bg3.scroll.y;
    case hardware::BG2PA:
      STUB_ADDR(bg2pa);
    case hardware::BG2PB:
      STUB_ADDR(bg2pb);
    case hardware::BG2PC:
      STUB_ADDR(bg2pc);
    case hardware::BG2PD:
      STUB_ADDR(bg2pd);
    case hardware::BG2X:
      STUB_ADDR(bg2x);
    case hardware::BG2Y:
      STUB_ADDR(bg2y);
    case hardware::BG3PA:
      STUB_ADDR(bg3pa);
    case hardware::BG3PB:
      STUB_ADDR(bg3pb);
    case hardware::BG3PC:
      STUB_ADDR(bg3pc);
    case hardware::BG3PD:
      STUB_ADDR(bg3pd);
    case hardware::BG3X:
      STUB_ADDR(bg3x);
    case hardware::BG3Y:
      STUB_ADDR(bg3y);
    case hardware::WIN0H:
      STUB_ADDR(win0h);
    case hardware::WIN1H:
      STUB_ADDR(win1h);
    case hardware::WIN0V:
      STUB_ADDR(win0v);
    case hardware::WIN1V:
      STUB_ADDR(win1v);
    case hardware::WININ:
      STUB_ADDR(winin);
    case hardware::WINOUT:
      STUB_ADDR(winout);
    case hardware::MOSAIC:
      STUB_ADDR(mosaic);
    case hardware::BLDCNT:
      STUB_ADDR(bldcnt);
    case hardware::BLDALPHA:
      STUB_ADDR(bldalpha);
    case hardware::BLDY:
      STUB_ADDR(bldy);
    case hardware::SOUND1CNT_L:
      STUB_ADDR(sound1cnt_l);
    case hardware::SOUND1CNT_H:
      STUB_ADDR(sound1cnt_h);
    case hardware::SOUND1CNT_X:
      STUB_ADDR(sound1cnt_x);
    case hardware::SOUND2CNT_L:
      STUB_ADDR(sound2cnt_l);
    case hardware::SOUND2CNT_H:
      STUB_ADDR(sound2cnt_h);
    case hardware::SOUND3CNT_L:
      STUB_ADDR(sound3cnt_l);
    case hardware::SOUND3CNT_H:
      STUB_ADDR(sound3cnt_h);
    case hardware::SOUND3CNT_X:
      STUB_ADDR(sound3cnt_x);
    case hardware::SOUND4CNT_L:
      STUB_ADDR(sound4cnt_l);
    case hardware::SOUND4CNT_H:
      STUB_ADDR(sound4cnt_h);
    case hardware::SOUNDCNT_L:
      STUB_ADDR(soundcnt_l);
    case hardware::SOUNDCNT_H:
      STUB_ADDR(soundcnt_h);
    case hardware::SOUNDCNT_X:
      STUB_ADDR(soundcnt_x);
    case hardware::SOUNDBIAS:
      STUB_ADDR(soundbias);
    case hardware::FIFO_A:
      STUB_ADDR(fifo_a);
    case hardware::FIFO_B:
      STUB_ADDR(fifo_b);
    case hardware::SIOMULTI0:
      STUB_ADDR(siomulti0);
    case hardware::SIOMULTI1:
      STUB_ADDR(siomulti1);
    case hardware::SIOMULTI2:
      STUB_ADDR(siomulti2);
    case hardware::SIOMULTI3:
      STUB_ADDR(siomulti3);
    case hardware::SIOCNT:
      STUB_ADDR(siocnt);
    case hardware::SIODATA8:
      STUB_ADDR(siodata8);
    case hardware::KEYCNT:
      STUB_ADDR(keycnt);
    case hardware::RCNT:
      STUB_ADDR(rcnt);
    case hardware::JOYCNT:
      STUB_ADDR(joycnt);
    case hardware::JOY_RECV:
      STUB_ADDR(joy_recv);
    case hardware::JOY_TRANS:
      STUB_ADDR(joy_trans);
    case hardware::JOYSTAT:
      STUB_ADDR(joystat);
    case hardware::IE:
      return hardware.cpu->interrupts_enabled;
    case hardware::IF:
      return hardware.cpu->interrupts_requested;
    case hardware::POSTFLG:
      STUB_ADDR(postflg);
  }

  if (const auto [address_type, dma_number, found] = Dma::select_dma(addr);
      found) {
    Dma& dma = hardware.dmas->dma(dma_number);
    switch (address_type) {
      case Dma::AddressType::Source:
        return dma.source;
      case Dma::AddressType::Dest:
        return dma.dest;
      case Dma::AddressType::Count:
        return dma.count;
      case Dma::AddressType::Control:
        return dma.control();
    }
  }

  fmt::printf("unimplemented io register %08x\n", addr);
  throw std::runtime_error("unimplemented io register switch");
}

}  // namespace gb::advance
