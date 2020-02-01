#pragma once
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
  u16 max_bank_mask = 1;
  u16 max_ram_bank_mask;
  u8 lower = 0x01;
  u8 upper = 0x00;

  u8 ram_bank = 0x00;

  bool enable_save_ram = false;

  bool mbc1_rom_mode = false;

  // std::size_t relative_ram_address(u16 addr) const { return addr - 0xa000; }

 public:
  static std::size_t relative_ram_address(u16 addr) { return addr - 0xa000; }
  Mbc(Type mbc_type, u16 max_rom_banks, u16 max_ram_banks)
      : type{mbc_type},
        max_bank_mask{max_rom_banks},
        max_ram_bank_mask{
            static_cast<u16>(max_ram_banks == 0 ? 1 : max_ram_banks)} {}

  void set_lower(u8 val);
  void set_upper(u8 val);

  bool handle_memory_write(u16 addr, u8 value);

  u16 lower_rom_bank_selected() const;
  u16 rom_bank_selected() const;

  void set_ram_bank(u8 val);

  bool in_ram_range(u16 addr) const;

  bool save_ram_enabled() const { return enable_save_ram; }

  void set_save_ram_enabled(bool value) { enable_save_ram = value; }

  u8 ram_bank_selected() const {
    if (type == Mbc::Type::MBC1 && !mbc1_rom_mode) {
      return 0;
    }
    return ram_bank % max_ram_bank_mask;
  }

  std::size_t absolute_ram_address(u16 addr) const {
    return absolute_ram_offset() + relative_ram_address(addr);
  }

  std::size_t absolute_ram_offset() const {
    return 0x2000 * ram_bank_selected();
  }
};
}  // namespace gb
