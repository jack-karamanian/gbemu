#pragma once
#include <doctest/doctest.h>
#include "types.h"
namespace gb {
class Mbc {
 public:
  enum class Type {
    None,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
  };

 private:
  Type type = Type::None;
  u8 lower = 0x00;
  u8 upper = 0x00;

  u8 ram_bank = 0x00;

 public:
  Mbc(Type mbc_type) : type{mbc_type} {}

  void set_lower(u8 val);
  void set_upper(u8 val);

  bool in_lower_write_range(u16 addr) const;
  bool in_upper_write_range(u16 addr) const;

  u16 rom_bank_selected() const;

  void set_ram_bank(u8 val);

  bool in_ram_enable_range(u16 addr) const;
  bool in_ram_bank_write_range(u16 addr) const;
  bool in_ram_range(u16 addr) const;

  u8 ram_bank_selected() const { return ram_bank; }

  std::size_t relative_ram_address(u16 addr) const { return addr - 0xa000; }

  std::size_t absolute_ram_offset() const { return 0x2000 * ram_bank; }

  std::size_t absolute_ram_address(u16 addr) const {
    return absolute_ram_offset() + relative_ram_address(addr);
  }
};
}  // namespace gb
