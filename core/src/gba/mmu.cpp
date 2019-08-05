#include <fmt/printf.h>
#include "gba/cpu.h"
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

static u32 next_addr(u32 addr, u32 size, Mmu::AddrOp op) {
  switch (op) {
    case Mmu::AddrOp::Increment:
      return addr + size;
    case Mmu::AddrOp::Decrement:
      return addr - size;
    case Mmu::AddrOp::Fixed:
      return addr;
  }
  GB_UNREACHABLE();
}

void Mmu::copy_memory(AddrParam source,
                      AddrParam dest,
                      u32 count,
                      u32 type_size) {
  const auto [source_addr, source_op] = source;
  const auto [dest_addr, dest_op] = dest;

  auto [source_storage, resolved_source_addr] =
      select_storage(source_addr, Mmu::DataOperation::Read);
  auto [dest_storage, resolved_dest_addr] =
      select_storage(dest_addr, Mmu::DataOperation::Write);

  for (u32 i = 0; i < count; ++i) {
    for (u32 j = 0; j < type_size; ++j) {
      dest_storage[resolved_dest_addr + j] =
          source_storage[resolved_source_addr + j];
    }
    resolved_source_addr =
        next_addr(resolved_source_addr, type_size, source_op);
    resolved_dest_addr = next_addr(resolved_dest_addr, type_size, dest_op);
  }
}

std::tuple<nonstd::span<u8>, u32> Mmu::select_storage(u32 addr,
                                                      DataOperation op) {
  if ((addr & 0xff000000) == 0x04000000) {
    return select_hardware(addr, op);
  }

  if (addr >= IWramBegin && addr <= IWramEnd) {
    return {iwram, addr - IWramBegin};
  }

  if (addr >= EWramBegin && addr <= EWramEnd) {
    return {ewram, addr - EWramBegin};
  }

  if (addr >= VramBegin && addr <= VramEnd) {
    return {vram, addr - VramBegin};
  }

  if (addr >= PaletteBegin && addr <= PaletteEnd) {
    return {palette_ram, addr - PaletteBegin};
  }

  if (addr >= OamBegin && addr <= OamEnd) {
    return {oam_ram, addr - OamBegin};
  }

  if (addr >= 0x03ffff00 && addr < 0x04000000) {
    const u32 offset = addr & 0xff;
    return {iwram, 0x7f00 + offset};
  }

  if (addr >= SramBegin && addr <= SramEnd) {
    return {sram, addr - SramBegin};
  }

  for (const auto [begin, end] : rom_regions) {
    if (addr >= begin && addr <= end) {
      return {rom, addr - begin};
    }
  }

  // Bios interrupt handler
  if (addr >= 0x00000128 && addr <= 0x0000013c) {
    return {bios_interrupt, addr - 0x128};
  }

  printf("unimplemented select_storage addr %08x\n", addr);
  throw std::runtime_error("unimplemented select storage");
}

void Mmu::handle_write_side_effects(u32 addr) {
  Dmas* dmas = hardware.dmas;
  if (addr >= Dma::Addresses<Dma::DmaNumber::Dma0>::Control &&
      addr < Dma::Addresses<Dma::DmaNumber::Dma0>::Control + 2) {
    dmas->dma(Dma::DmaNumber::Dma0).handle_side_effects();
  } else if (addr >= Dma::Addresses<Dma::DmaNumber::Dma1>::Control &&
             addr < Dma::Addresses<Dma::DmaNumber::Dma1>::Control + 2) {
    dmas->dma(Dma::DmaNumber::Dma1).handle_side_effects();
  } else if (addr >= Dma::Addresses<Dma::DmaNumber::Dma2>::Control &&
             addr < Dma::Addresses<Dma::DmaNumber::Dma2>::Control + 2) {
    dmas->dma(Dma::DmaNumber::Dma2).handle_side_effects();
  } else if (addr >= Dma::Addresses<Dma::DmaNumber::Dma3>::Control &&
             addr < Dma::Addresses<Dma::DmaNumber::Dma3>::Control + 2) {
    dmas->dma(Dma::DmaNumber::Dma3).handle_side_effects();
  }
}

struct IntegerBox {
  template <typename T,
            typename = std::enable_if_t<
                std::is_base_of_v<Integer<u32>, std::decay_t<T>> ||
                std::is_base_of_v<Integer<u16>, std::decay_t<T>>>>
  IntegerBox(T& integer) : bytes{integer.byte_span()} {}

  IntegerBox(u32& num) : bytes{to_bytes(num)} {}

  IntegerBox(u16& num) : bytes{to_bytes(num)} {}

  nonstd::span<u8> bytes;
};

std::tuple<nonstd::span<u8>, u32> Mmu::select_hardware(u32 addr,
                                                       DataOperation op) {
  const auto [io_addr, offset] = select_io_register(addr);

  IntegerBox box = [io_addr = io_addr, addr, op, this]() -> IntegerBox {
    switch (io_addr) {
      case hardware::DISPCNT:
        return dispcnt;
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
        STUB_ADDR(bg0cnt);
      case hardware::BG1CNT:
        STUB_ADDR(bg1cnt);
      case hardware::BG2CNT:
        STUB_ADDR(bg2cnt);
      case hardware::BG3CNT:
        STUB_ADDR(bg3cnt);
      case hardware::BG0HOFS:
        STUB_ADDR(bg0hofs);
      case hardware::BG0VOFS:
        STUB_ADDR(bg0vofs);
      case hardware::BG1HOFS:
        STUB_ADDR(bg1hofs);
      case hardware::BG1VOFS:
        STUB_ADDR(bg1vofs);
      case hardware::BG2HOFS:
        STUB_ADDR(bg2hofs);
      case hardware::BG2VOFS:
        STUB_ADDR(bg2vofs);
      case hardware::BG3HOFS:
        STUB_ADDR(bg3hofs);
      case hardware::BG3VOFS:
        STUB_ADDR(bg3vofs);
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

    if (const auto [address_type, dma_number, found] = Dma::select_dma(io_addr);
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
  }();
  return {box.bytes, offset};
}
}  // namespace gb::advance
