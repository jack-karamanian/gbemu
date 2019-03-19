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

 public:
  void set_type(Type mbc_type) { type = mbc_type; }
  void set_lower(u8 val);
  void set_upper(u8 val);

  bool in_lower_write_range(u16 addr) const;
  bool in_upper_write_range(u16 addr) const;

  u16 get_rom_bank_selected() const;
};
}  // namespace gb
