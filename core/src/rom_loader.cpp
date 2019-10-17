#include <fmt/printf.h>
#include "rom_loader.h"

namespace gb {
RomHeader parse_rom(nonstd::span<const u8> data) {
  // Decode the MBC type from the cart header
  const Mbc::Type mbc_type = [rom_type = data[0x147]] {
    fmt::printf("rom type: %d\n", rom_type);
    switch (rom_type) {
      case 0x00:
        return Mbc::Type::None;
        break;
      case 0x01:
      case 0x02:
      case 0x03:
        return Mbc::Type::MBC1;
        break;
      case 0x05:
      case 0x06:
        return Mbc::Type::MBC2;
        break;
      case 0x0f:
      case 0x10:
      case 0x11:
      case 0x12:
      case 0x13:
        return Mbc::Type::MBC3;
        break;
      case 0x19:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
        return Mbc::Type::MBC5;
        break;
      default:
        return Mbc::Type::None;
    }
  }();

  const int save_ram_size = [ram_size = data[0x149], mbc_type] {
    if (mbc_type == Mbc::Type::MBC2) {
      return 256;
    }
    switch (ram_size) {
      case 0:
        return 0;
      case 1:
        return 2 * 1024;
      case 2:
        return 8 * 1024;
      case 3:
        return 32 * 1024;
      case 4:
        return 128 * 1024;
      case 5:
        return 64 * 1024;
      default:
        throw std::runtime_error("invalid save ram size");
    }
  }();

  const unsigned int rom_size = (1024 * 32) << data[0x148];
  fmt::printf("%d\n", data[0x148]);
  fmt::printf("%d\n", data[0x149]);

  const u8 gbc_flag = data[0x143];
  const bool is_gbc = gbc_flag == 0xc0 || gbc_flag == 0x80;

  Mbc mbc{mbc_type, static_cast<u16>(rom_size / 16384),
          static_cast<u16>(save_ram_size / 8192)};

  return {mbc, rom_size, save_ram_size, is_gbc};
}
}  // namespace gb
