#include <initializer_list>
#include <vector>
#include "mbc.h"

namespace gb {

void Mbc::set_lower(u8 val) {
  switch (type) {
    case Mbc::Type::MBC1:
      lower = val & 0x1f;
      break;
    case Mbc::Type::MBC2:
      lower = val & 0x0f;
      break;
    case Mbc::Type::MBC3:
      lower = val & 0x7f;
      break;
    case Mbc::Type::MBC5:
      lower = val;
      break;
    case Mbc::Type::None:
      // No effect
      break;
  }
}

void Mbc::set_upper(u8 val) {
  switch (type) {
    case Mbc::Type::MBC1:
      upper = val & 0x03;
      break;
    case Mbc::Type::MBC5:
      upper = val & 0x01;
      break;
    case Mbc::Type::None:
    case Mbc::Type::MBC2:
    case Mbc::Type::MBC3:
      // No effect
      break;
  }
}

bool Mbc::in_lower_write_range(u16 addr) const {
  const u16 masked_addr = addr & 0xf000;
  switch (type) {
    case Mbc::Type::MBC1:
    case Mbc::Type::MBC2:
    case Mbc::Type::MBC3:
      return masked_addr == 0x2000 || masked_addr == 0x3000;
    case Mbc::Type::MBC5:
      return masked_addr == 0x2000;
    default:
      return false;
  }
}

bool Mbc::in_upper_write_range(u16 addr) const {
  const u16 masked_addr = addr & 0xf000;
  switch (type) {
    case Mbc::Type::MBC1:
      return masked_addr == 0x4000 || masked_addr == 0x5000;
    case Mbc::Type::MBC5:
      return masked_addr == 0x3000;
    default:
      return false;
  }
}

u16 Mbc::rom_bank_selected() const {
  const int upper_shift = type == Mbc::Type::MBC5 ? 8 : 5;
  const u16 bank = static_cast<u16>(upper << upper_shift) | lower;
  if (bank == 0) {
    return type == Mbc::Type::MBC5 ? 0 : 1;
  }
  if (type != Mbc::Type::MBC3 &&
      (bank == 0x20 || bank == 0x40 || bank == 0x60)) {
    return bank + 1;
  }
  return bank;
}

void Mbc::set_ram_bank(u8 val) {
  switch (type) {
    case Mbc::Type::MBC1:
    case Mbc::Type::MBC3:
      ram_bank = val & 0x03;
      break;
    case Mbc::Type::MBC5:
      ram_bank = val & 0x0f;
      break;
    default:
      break;
  }
}

bool Mbc::in_ram_enable_range(u16 addr) const {
  return addr >= 0x0000 && addr <= 0x1fff;
}

bool Mbc::in_ram_bank_write_range(u16 addr) const {
  switch (type) {
    case Mbc::Type::MBC3:
      if (ram_bank > 3) {
        return false;
      }
    case Mbc::Type::MBC1:
    case Mbc::Type::MBC5:
      return addr >= 0x4000 && addr <= 0x5fff;
    default:
      return false;
  }
}

bool Mbc::in_ram_range(u16 addr) const {
  switch (type) {
    case Mbc::Type::MBC2:
      return addr >= 0xa000 && addr <= 0xa1ff;
    case Mbc::Type::MBC1:
    case Mbc::Type::MBC3:
    case Mbc::Type::MBC5:
      return addr >= 0xa000 && addr <= 0xbfff;
    default:
      return false;
  }
}

TEST_CASE("Mbc::rom_bank_selected") {
  SUBCASE("all types should allow the maximum rom bank to be selected") {
    std::vector<std::pair<Mbc::Type, u16>> max_bank_for_type{
        {Mbc::Type::MBC1, 0x007f},
        {Mbc::Type::MBC2, 0x000f},
        {Mbc::Type::MBC3, 0x007f},
        {Mbc::Type::MBC5, 0x01ff},
    };

    for (const auto [type, max_bank] : max_bank_for_type) {
      Mbc mbc{type};
      mbc.set_lower(0xff);
      mbc.set_upper(0xff);
      CAPTURE(type);
      CAPTURE(max_bank);
      CHECK(mbc.rom_bank_selected() == max_bank);
    }
  }

  SUBCASE("bank 0 should be correct for all types (0 for MBC5, 1 for others)") {
    for (auto type : {Mbc::Type::None, Mbc::Type::MBC1, Mbc::Type::MBC2,
                      Mbc::Type::MBC3, Mbc::Type::MBC5}) {
      Mbc mbc{type};

      if (type == Mbc::Type::MBC5) {
        CHECK(mbc.rom_bank_selected() == 0);
      } else {
        CHECK(mbc.rom_bank_selected() == 1);
      }
    }
  }
}

TEST_CASE("Mbc") {
  SUBCASE("write ranges") {
    SUBCASE("Mbc::in_upper_write_range") {
      auto check_in_upper_range = [](const Mbc& bank, int begin, int end) {
        for (int i = 0; i <= 0xffff; i++) {
          if (i >= begin && i <= end) {
            CHECK(bank.in_upper_write_range(i));
          } else {
            CHECK(!bank.in_upper_write_range(i));
          }
        }
      };
      SUBCASE("MBC1 should return true for addrs 0x4000-0x5fff") {
        Mbc bank{Mbc::Type::MBC1};

        check_in_upper_range(bank, 0x4000, 0x5fff);
      }
      SUBCASE("MBC5 should return true for addrs 0x3000-0x3fff") {
        Mbc bank{Mbc::Type::MBC5};
        check_in_upper_range(bank, 0x3000, 0x3fff);
      }
      SUBCASE("all other banks should return false for all addrs") {
        for (auto type : {Mbc::Type::MBC2, Mbc::Type::MBC3}) {
          Mbc bank{type};
          check_in_upper_range(bank, -1, -1);
        }
      }
    }

    SUBCASE("Mbc::in_lower_write_range") {
      auto check_in_lower_range = [](const Mbc& bank, int begin, int end) {
        for (int i = 0; i <= 0xffff; i++) {
          if (i >= begin && i <= end) {
            CHECK(bank.in_lower_write_range(i));
          } else {
            CHECK(!bank.in_lower_write_range(i));
          }
        }
      };
      SUBCASE("no MBC should return false for all addrs") {
        Mbc bank{Mbc::Type::None};
        check_in_lower_range(bank, -1, -1);
      }

      SUBCASE(
          "MBC1, MBC2, and MBC3 should return true for addrs 0x2000-0x3fff") {
        for (auto type : {Mbc::Type::MBC1, Mbc::Type::MBC2, Mbc::Type::MBC3}) {
          Mbc bank{type};
          check_in_lower_range(bank, 0x2000, 0x3fff);
        }
      }

      SUBCASE("MBC5 should return true for addrs 0x2000-0x2fff") {
        Mbc bank{Mbc::Type::MBC5};
        check_in_lower_range(bank, 0x2000, 0x2fff);
      }
    }
  }
}
}  // namespace gb
