#include "gba/mmu.h"
#include <fmt/printf.h>
#include <chrono>
#include <numeric>
#include "gba/cpu.h"
#include "gba/gpu.h"
#include "gba/sound.h"
#include "timer.h"
#include "utils.h"

#define STUB_ADDR(name)  \
  do {                   \
    static u32 name = 0; \
    return name;         \
  } while (0)

namespace gb::advance {

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

static void unpack_bits(nonstd::span<u8> dest, const u64 value) {
  u16 bit_value = 0;
  fmt::printf("%016x\n", value);
  for (unsigned int i = 0; i < sizeof(u64) * 8; ++i) {
    const u32 shift = 63 - i;
    bit_value = (value & (1ULL << shift)) >> shift;
    fmt::print("{} ", bit_value);
    std::memcpy(&dest[i * 2], &bit_value, sizeof(u16));
  }
  fmt::print("\n");
}

void Mmu::eeprom_send_command(nonstd::span<const u8> source_storage,
                              u32 count) {
  constexpr u32 type_size = 2;

  const auto resolved_source_storage =
      source_storage.subspan(0, count * type_size);

  fmt::print("EEPROM DATA ");
  for (unsigned int i = 0; i < count * type_size; ++i) {
    fmt::printf("%02x ", source_storage[i]);
  }
  fmt::print("\n");

  const bool read_command = [resolved_source_storage] {
    const u8 upper_bit = resolved_source_storage[0] & 1;
    const u8 lower_bit = resolved_source_storage[2] & 1;
    assert(!(upper_bit == 0 && lower_bit == 0));
    if (upper_bit == 0 && lower_bit == 0) {
      throw std::runtime_error("invalid eeprom transfer header found 00");
    }
    return (upper_bit == 1 && lower_bit == 1);
  }();

  const auto read_addr = [resolved_source_storage](unsigned int size) -> u16 {
    const auto addr_bytes = resolved_source_storage.subspan(4, size * 2);

    u16 res = 0;
    for (unsigned int i = 0; i < size; ++i) {
      res |= (addr_bytes[i * 2] & 1) << (size - (i + 1));
    }

    assert((size == 6 && res <= 0x3f) || (size == 14 && res <= 0x3ff));
    return res;
  };

  if (read_command) {
    const int eeprom_addr_size = count == 9 ? 6 : 14;
    const u16 eeprom_addr = read_addr(eeprom_addr_size);
    u8* const eeprom_storage = &m_eeprom[sizeof(u64) * eeprom_addr];

    fmt::printf("READ ADDR %04x\n", eeprom_addr);

    std::memcpy(&m_eeprom_buffer, eeprom_storage, sizeof(u64));
  } else {
    const int eeprom_addr_size = count == 73 ? 6 : 14;
    const u16 eeprom_addr = read_addr(eeprom_addr_size);
    fmt::printf("WRITE ADDR %04x\n", eeprom_addr);
    u8* const eeprom_storage = &m_eeprom[sizeof(u64) * eeprom_addr];
    // Write
    const nonstd::span<const u8> write_data = resolved_source_storage.subspan(
        4 + (sizeof(u16) * eeprom_addr_size), 64 * sizeof(u16));

    u64 data = 0;
    for (int i = 0; i < 64; ++i) {
      data |= (write_data[2 * i] & 1ULL) << (63 - i);
    }

    std::memcpy(eeprom_storage, &data, sizeof(u64));
  }
}

void Mmu::eeprom_read(nonstd::span<u8> dest) {
  unpack_bits(dest.subspan(4 * sizeof(u16)), m_eeprom_buffer);
}

void Mmu::copy_memory(AddrParam source,
                      AddrParam dest,
                      u32 count,
                      u32 type_size) {
  const auto [source_addr, source_op] = source;
  const auto [dest_addr, dest_op] = dest;

  if ((source_addr & 0xff000000) == 0 || (dest_addr & 0xff000000) == 0) {
    return;
  }

  if (memory_region(dest_addr) == 0x0d000000 ||
      memory_region(source_addr) == 0x0d000000) {
    if (type_size != 2) {
      throw std::runtime_error("unexpected type size in eeprom write");
    }

    if (!m_eeprom_enabled) {
      m_memory_region_table[0xd] = m_eeprom;
      m_eeprom_enabled = true;
    }

    if (memory_region(dest_addr) == 0x0d000000) {
      const auto [source_storage, resolved_source_addr] =
          select_storage(source_addr);
      eeprom_send_command(source_storage.subspan(resolved_source_addr), count);
    } else if (memory_region(source_addr) == 0x0d000000) {
      const auto [dest_storage, resolved_dest_addr] = select_storage(dest_addr);
      eeprom_read(dest_storage.subspan(resolved_dest_addr));
    }
    return;
  }

  const int source_stride =
      static_cast<int>(source_op) * static_cast<int>(type_size);
  const int dest_stride =
      static_cast<int>(dest_op) * static_cast<int>(type_size);
  if (is_hardware_addr(source_addr) || is_hardware_addr(dest_addr)) {
    u32 resolved_source_addr = source_addr;
    u32 resolved_dest_addr = dest_addr;
    for (u32 i = 0; i < count; ++i) {
      for (u32 j = 0; j < type_size; ++j) {
        set<u8>(resolved_dest_addr + (dest_stride == 0 ? 0 : j),
                at<u8>(resolved_source_addr + j));
      }
      resolved_source_addr += source_stride;
      resolved_dest_addr += dest_stride;
    }
  } else {
    auto [source_storage, resolved_source_addr] = select_storage(source_addr);
    auto [dest_storage, resolved_dest_addr] = select_storage(dest_addr);

    for (u32 i = 0; i < count; ++i) {
      for (u32 j = 0; j < type_size; ++j) {
        dest_storage[resolved_dest_addr + (dest_stride == 0 ? 0 : j)] =
            source_storage[resolved_source_addr + j];
      }
      resolved_source_addr += source_stride;
      resolved_dest_addr += dest_stride;
    }
  }
}

nonstd::span<const u8> Mmu::get_prefetched_opcode() const noexcept {
  return hardware.cpu->prefetched_opcode();
}

void Mmu::set_bytes(u32 addr, nonstd::span<const u8> bytes) {
  auto [selected_span, resolved_addr] = select_storage(addr);
  auto subspan = selected_span.subspan(resolved_addr);

  const std::size_t copy_size = bytes.size() & 7;

  for (std::size_t i = 0; i < copy_size; ++i) {
    subspan[i] = bytes[i];
  }
  if (m_write_handler) {
    // m_write_handler(addr, 0);
  }
}

void Mmu::set_hardware_bytes(u32 addr, nonstd::span<const u8> bytes) {
  const auto [io_addr, resolved_addr] = select_io_register(addr);
  auto selected_hardware = select_hardware(io_addr, DataOperation::Write);

  const auto bytes_size = static_cast<std::size_t>(bytes.size() & 7);

  const bool is_overrunning = bytes_size > selected_hardware.size_bytes();
  const long difference =
      bytes_size - (selected_hardware.size_bytes() - resolved_addr);

  assert(io_addr + resolved_addr == addr);
  selected_hardware.write_byte(
      resolved_addr, difference > 0 ? bytes.subspan(0, difference) : bytes);

  if (is_overrunning) {
    set_hardware_bytes(addr + difference, bytes.subspan(difference));
  }

#if 0
  if (m_write_handler) {
    m_write_handler(addr, 0);
  }
#endif
}

void Mmu::print_bios_warning() const {
#if 1
  fmt::printf(
      "WARNING: BIOS memory access at %08x\n",
      hardware.cpu->reg(Register::R15) - hardware.cpu->prefetch_offset());
#endif
}

class MgbaDebugPrint : public Integer<u16> {
 public:
  constexpr MgbaDebugPrint() : Integer::Integer{0} {}

  void write_byte([[maybe_unused]] u32 addr, u8 value) {
    std::putc(value, stdout);
  }
};

static MgbaDebugPrint mgba_debug_print;

IntegerRef Mmu::select_hardware(u32 addr, DataOperation op) {
  switch (addr) {
    case hardware::mgba::DEBUG_ENABLE:
      STUB_ADDR(mgba_debug_enable);
    case hardware::mgba::DEBUG_FLAGS:
      STUB_ADDR(mgba_debug_flags);
    case hardware::mgba::DEBUG_STRING:
      return mgba_debug_print;
    case hardware::GREENSWAP: {
      static u16 green_swap = 0;
      return green_swap;
    }
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
      return hardware.gpu->bg2.affine_matrix[0];
    case hardware::BG2PB:
      return hardware.gpu->bg2.affine_matrix[1];
    case hardware::BG2PC:
      return hardware.gpu->bg2.affine_matrix[2];
    case hardware::BG2PD:
      return hardware.gpu->bg2.affine_matrix[3];
    case hardware::BG2X:
      return hardware.gpu->bg2.affine_scroll.x;
    case hardware::BG2Y:
      return hardware.gpu->bg2.affine_scroll.y;
    case hardware::BG3PA:
      return hardware.gpu->bg3.affine_matrix[0];
    case hardware::BG3PB:
      return hardware.gpu->bg3.affine_matrix[1];
    case hardware::BG3PC:
      return hardware.gpu->bg3.affine_matrix[2];
    case hardware::BG3PD:
      return hardware.gpu->bg3.affine_matrix[3];
    case hardware::BG3X:
      return hardware.gpu->bg3.affine_scroll.x;
    case hardware::BG3Y:
      return hardware.gpu->bg3.affine_scroll.y;
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
      return hardware.gpu->bldcnt;
    case hardware::BLDALPHA:
      return hardware.gpu->bldalpha;
    case hardware::BLDY:
      return hardware.gpu->bldy;
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
      return hardware.sound->soundcnt_high;
    case hardware::SOUNDCNT_X:
      STUB_ADDR(soundcnt_x);
    case hardware::SOUNDBIAS:
      return hardware.sound->soundbias;
    case hardware::FIFO_A:
      return hardware.sound->fifo_a;
    case hardware::FIFO_B:
      return hardware.sound->fifo_b;
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
    case hardware::KEYCNT: {
      static u16 keycnt = 0;
      return keycnt;
    }
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
    case hardware::DMA0SAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma0).source;
    case hardware::DMA1SAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma1).source;
    case hardware::DMA2SAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma2).source;
    case hardware::DMA3SAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma3).source;
    case hardware::DMA0DAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma0).dest;
    case hardware::DMA1DAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma1).dest;
    case hardware::DMA2DAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma2).dest;
    case hardware::DMA3DAD:
      return hardware.dmas->dma(Dma::DmaNumber::Dma3).dest;
    case hardware::DMA0CNT_L:
      return hardware.dmas->dma(Dma::DmaNumber::Dma0).count;
    case hardware::DMA1CNT_L:
      return hardware.dmas->dma(Dma::DmaNumber::Dma1).count;
    case hardware::DMA2CNT_L:
      return hardware.dmas->dma(Dma::DmaNumber::Dma2).count;
    case hardware::DMA3CNT_L:
      return hardware.dmas->dma(Dma::DmaNumber::Dma3).count;
    case hardware::DMA0CNT_H:
      return hardware.dmas->dma(Dma::DmaNumber::Dma0).control();
    case hardware::DMA1CNT_H:
      return hardware.dmas->dma(Dma::DmaNumber::Dma1).control();
    case hardware::DMA2CNT_H:
      return hardware.dmas->dma(Dma::DmaNumber::Dma2).control();
    case hardware::DMA3CNT_H:
      return hardware.dmas->dma(Dma::DmaNumber::Dma3).control();
    case hardware::WAVERAM:
      STUB_ADDR(waveram);
  }

  fmt::printf("unimplemented io register %08x\n", addr);
  throw std::runtime_error("unimplemented io register switch");
}

}  // namespace gb::advance
