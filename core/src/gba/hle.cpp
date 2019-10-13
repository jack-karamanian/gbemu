#include "gba/hle.h"
#include <doctest/doctest.h>
#include "gba/mmu.h"

namespace gb::advance::hle::bios {
static float float_from_fixed(s16 num) noexcept {
  return static_cast<float>(num) / (1 << 14);
}
static s16 fixed_from_float(float num) noexcept {
  return static_cast<s16>(num * (1 << 14));
}

void cpu_set(Mmu& mmu, u32 source, u32 dest, u32 control) {
  const u32 type_size = gb::test_bit(control, 26) ? 4 : 2;
  const u32 count = control & 0x1fffff;

  const bool fixed_source = gb::test_bit(control, 24);

  mmu.copy_memory(
      {source, fixed_source ? Mmu::AddrOp::Fixed : Mmu::AddrOp::Increment},
      {dest, Mmu::AddrOp::Increment}, count, type_size);
}

void cpu_fast_set(Mmu& mmu, u32 source, u32 dest, u32 control) {
  const u32 count = control & 0x1fffff;
  const bool fixed_source = gb::test_bit(control, 24);

  mmu.copy_memory(
      {source, fixed_source ? Mmu::AddrOp::Fixed : Mmu::AddrOp::Increment},
      {dest, Mmu::AddrOp::Increment}, count, 4);
}

s16 arctan2(s16 x, s16 y) noexcept {
  const float floating_x = float_from_fixed(x);
  const float floating_y = float_from_fixed(y);
  const float res = std::atan2(floating_y, floating_x);
  return fixed_from_float(res);
}

void lz77_decompress(nonstd::span<const u8> src,
                     nonstd::span<u8> dest,
                     u32 type_size) {
  const u32 data_size = (src[3] << 16) | (src[2] << 8) | (src[1] << 0);
  // fmt::print("data size {}\n", data_size);

  u32 source_addr = 0;
  u32 dest_addr = 0;

  auto write_byte = [dest, stored_halfword = 0](u32 addr, u8 value) mutable {
    if ((addr & 1) != 0) {
      // fmt::print("store\n");
      // stored_halfword = (stored_halfword & 0x00ff) | (value << 8);
      dest[addr] = value;
      dest[addr - 1] = stored_halfword & 0xff;
    } else {
      stored_halfword = (stored_halfword & 0xff00) | value;
      // fmt::print("stage\n");
    }
  };

  const auto compressed_data = src.subspan(4);
  while (dest_addr < data_size) {
    // fmt::print("{}\n", dest_addr);
    const u8 flags = compressed_data[source_addr++];

    for (u8 mask = 0b1000'0000; mask != 0; mask >>= 1) {
      const bool uncompressed = (flags & mask) == 0;

      if (uncompressed) {
        if (type_size == 2) {
          write_byte(dest_addr, compressed_data[source_addr]);
        } else {
          dest[dest_addr] = compressed_data[source_addr];
        }
        ++source_addr;
        ++dest_addr;
      } else {
        // fmt::printf("source addr: %08x dest_addr: %08x\n", source_addr,
        //            dest_addr);
        const u16 block = (compressed_data[source_addr + 1] << 8) |
                          compressed_data[source_addr];
        const u16 displacement =
            dest_addr - (((block & 0b1111) << 8) | (((block >> 8) & 0xff))) - 1;
        const u32 count = ((block >> 4) & 0b1111) + 3;

        if (type_size == 1) {
          std::copy(&dest[displacement], &dest[displacement + count],
                    &dest[dest_addr]);
        } else {
          // fmt::print("{} {}\n", displacement, dest_addr);

          for (u32 i = 0U; i < count + 1; ++i) {
            write_byte(dest_addr + i, dest[displacement + i]);
          }
        }

        dest_addr += count;
        source_addr += 2;
      }

      if (dest_addr >= data_size) {
        return;
      }
    }
  }
}

TEST_CASE("lz77_decompress should work") {
  using namespace std::literals;
  constexpr auto expected = "abracadabra"sv;
  constexpr std::array<u8, 16> compressed = {0x10, 0x0b, 0x00, 0x00, 0x01, 0x61,
                                             0x62, 0x72, 0x61, 0x63, 0x61, 0x64,
                                             0x10, 0x06, 0x00, 0x00};
  std::array<u8, 12> output{};

  lz77_decompress(compressed, output, 2);

  CHECK(std::string_view{reinterpret_cast<char*>(output.data()),
                         output.size() - 1} == expected);
}

}  // namespace gb::advance::hle::bios
