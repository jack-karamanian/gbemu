#include "gba/thumb_instructions.h"
#include "gba/common_instructions.h"
#include "gba/cpu.h"
#include "utils.h"

namespace gb::advance {
namespace thumb {
template <ShiftType shift_type, u32 shift_amount>
u32 move_shifted_register(Cpu& cpu, u16 instruction) {
  const auto dest_reg = static_cast<Register>(instruction & 0b111);
  const auto src_reg = static_cast<Register>((instruction >> 3) & 0b111);

  const u32 value = cpu.reg(src_reg);
  common::run_shift(cpu, shift_type, dest_reg, value, shift_amount, false);

  return 1;
}

template <bool immediate_operand, bool subtract, u32 register_or_value>
u32 add_subtract(Cpu& cpu, u16 instruction) {
  const auto dest_reg = static_cast<Register>(instruction & 0b111);
  const auto src_reg = static_cast<Register>((instruction >> 3) & 0b111);

  const u32 value = immediate_operand
                        ? register_or_value
                        : cpu.reg(static_cast<Register>(register_or_value));

  common::data_processing<subtract ? Opcode::Sub : Opcode::Add, true>(
      cpu, dest_reg, cpu.reg(src_reg), value, cpu.program_status().carry());
  return 1;
}

template <Opcode opcode, Register dest_reg>
u32 move_compare_add_subtract_immediate(Cpu& cpu, u16 instruction) {
  const u32 immediate = instruction & 0xff;
  common::data_processing<opcode, true>(cpu, dest_reg, cpu.reg(dest_reg),
                                        immediate,
                                        cpu.program_status().carry());
  return 1;
}

template <u32 opcode>
u32 alu_operation(Cpu& cpu, u16 instruction) {
  const auto dest_reg = static_cast<Register>(instruction & 0b111);
  const auto src_reg = static_cast<Register>((instruction >> 3) & 0b111);

  if constexpr (opcode == 0b1101) {
    // Mul
    const auto cycles =
        multiply::detail::multiply_cycles(cpu.reg(dest_reg), false);
    common::multiply(cpu, dest_reg, src_reg, dest_reg, true);
    return cycles.sum();

  } else if constexpr ((opcode >= 0b0010 && opcode <= 0b0100) ||
                       opcode == 0b0111) {
    // Shift
    constexpr ShiftType shift_type = []() -> ShiftType {
      switch (opcode) {
        case 0b0010:
          return ShiftType::LogicalLeft;
        case 0b0011:
          return ShiftType::LogicalRight;
        case 0b0100:
          return ShiftType::ArithmeticRight;
        case 0b0111:
          return ShiftType::RotateRight;
      }
    }();

    common::run_shift(cpu, shift_type, dest_reg, cpu.reg(dest_reg),
                      cpu.reg(src_reg), true);
  } else {
    // clang-format off
    static constexpr std::array opcode_table = {
        Opcode::And,
        Opcode::Eor,
        Opcode::Mov, // LSL
        Opcode::Mov, // LSR
        Opcode::Mov, // ASR
        Opcode::Adc,
        Opcode::Sbc,
        Opcode::Mov, // ROR
        Opcode::Tst,
        Opcode::Rsb,
        Opcode::Cmp,
        Opcode::Cmn,
        Opcode::Orr,
        Opcode::Mov, // MUL
        Opcode::Bic,
        Opcode::Mvn,
    };
    // clang-format on
    constexpr auto translated_opcode = opcode_table[opcode];
    static_assert(translated_opcode != Opcode::Mov);

    common::data_processing<translated_opcode, true>(
        cpu, dest_reg,
        cpu.reg(translated_opcode == Opcode::Rsb ? src_reg : dest_reg),
        translated_opcode == Opcode::Rsb ? 0 : cpu.reg(src_reg),
        cpu.program_status().carry());
  }

  return 1;
}

template <int opcode, bool hi_dest, bool hi_src>
u32 hi_register_operation(Cpu& cpu, u16 instruction) {
  const auto src_reg =
      static_cast<Register>(((instruction >> 3) & 0b111) + hi_src * 8);
  if constexpr (opcode == 0b11) {
    // BX
    common::branch_and_exchange(cpu, src_reg);
    static_cast<void>(instruction);
    return 3;
  } else {
    const auto dest_reg =
        static_cast<Register>((instruction & 0b111) + hi_dest * 8);
    static constexpr std::array opcode_table = {Opcode::Add, Opcode::Cmp,
                                                Opcode::Mov};
    constexpr auto translated_opcode = opcode_table[opcode];

    common::data_processing<translated_opcode,
                            translated_opcode == Opcode::Cmp>(
        cpu, dest_reg, cpu.reg(dest_reg), cpu.reg(src_reg),
        cpu.program_status().carry());
  }
  return 2;
}

template <Register dest_reg>
u32 pc_relative_load(Cpu& cpu, u16 instruction) {
  const u32 offset = (instruction & 0xff) << 2;

  const u32 addr = ((cpu.reg(Register::R15) & ~0b10) + offset);

  cpu.set_reg(dest_reg, cpu.mmu()->at<u32>(addr));

  return cpu.mmu()->wait_cycles(addr, load_store_cycles(dest_reg, true));
}

template <bool load,
          bool register_offset,
          u32 register_or_offset,
          typename Type>
u32 load_store(Cpu& cpu, u16 instruction) {
  const u32 offset = [&cpu]() -> u32 {
    if constexpr (register_offset) {
      return cpu.reg(static_cast<Register>(register_or_offset));
    }
    static_cast<void>(cpu);
    return register_or_offset;
  }();

  const auto base_reg = static_cast<Register>((instruction >> 3) & 0b111);
  const auto dest_reg = static_cast<Register>(instruction & 0b111);

  const u32 addr = cpu.reg(base_reg) + offset;

  constexpr auto mask = sizeof(Type) - 1;

  const u32 aligned_addr = addr & ~mask;
  const u32 resolved_addr = aligned_addr;
  if constexpr (load) {
    run_load<Type>(cpu, addr, dest_reg);
  } else {
    cpu.mmu()->set(resolved_addr, static_cast<Type>(cpu.reg(dest_reg)));
  }
  return cpu.mmu()->wait_cycles(resolved_addr,
                                load_store_cycles(dest_reg, load));
}

template <bool load, Register dest_reg>
u32 sp_relative_load_store(Cpu& cpu, u16 instruction) {
  const u32 addr = cpu.reg(Register::R13) + ((instruction & 0xff) << 2);

  if constexpr (load) {
    const auto value = cpu.mmu()->at<u32>(addr & ~0b11);
    cpu.set_reg(dest_reg, rotate_right(value, (addr & 0b11) * 8));
  } else {
    cpu.mmu()->set(addr, cpu.reg(dest_reg));
  }

  return cpu.mmu()->wait_cycles(addr, load_store_cycles(dest_reg, load));
}

template <bool sp, Register dest_reg>
u32 load_address(Cpu& cpu, u16 instruction) {
  constexpr auto base_reg = sp ? Register::R13 : Register::R15;
  const u32 offset = ((instruction & 0xff) << 2);

  const u32 value =
      cpu.reg(base_reg) & (base_reg == Register::R15 ? ~0b10 : ~0);

  cpu.set_reg(dest_reg, value + offset);

  return 1;
}

template <bool negative>
u32 add_offset_to_stack_pointer(Cpu& cpu, u16 instruction) {
  const u32 offset = ((instruction & 0b111111) << 2);

  const u32 sp_value = cpu.reg(Register::R13);
  if constexpr (negative) {
    cpu.set_reg(Register::R13, sp_value - offset);
  } else {
    cpu.set_reg(Register::R13, sp_value + offset);
  }
  return 1;
}

template <bool load, bool load_pc_or_store_lr>
u32 push_pop_registers(Cpu& cpu, u16 instruction) {
  u32 sp = cpu.reg(Register::R13);
  u32 cycles = 0;

  const auto add_cycles = [&cycles, &cpu](u32 addr, Register reg) {
    cycles += cpu.mmu()->wait_cycles(addr, load_store_cycles(reg, load));
  };

  if constexpr (load) {
    // POP/LDMIA
    for (int i = 0; i < 8; ++i) {
      if (test_bit(instruction, i)) {
        const auto reg = static_cast<Register>(i);
        add_cycles(sp, reg);
        cpu.set_reg(reg, cpu.mmu()->at<u32>(sp));
        sp += 4;
      }
    }
    if constexpr (load_pc_or_store_lr) {
      add_cycles(sp, Register::R15);
      cpu.set_reg(Register::R15, cpu.mmu()->at<u32>(sp));
      sp += 4;
    }
  } else {
    // PUSH/STMDB
    if constexpr (load_pc_or_store_lr) {
      sp -= 4;
      cpu.mmu()->set(sp, cpu.reg(Register::R14));
      add_cycles(sp, Register::R14);
    }
    for (int i = 7; i >= 0; --i) {
      if (test_bit(instruction, i)) {
        sp -= 4;
        const auto reg = static_cast<Register>(i);
        cpu.mmu()->set(sp, cpu.reg(reg));
        add_cycles(sp, reg);
      }
    }
  }
  cpu.set_reg(Register::R13, sp);
  return cycles;
}

static int popcount(int num) {
#ifdef __GNUC__
  return __builtin_popcount(num);
#else
  int i = 0;
  while (num) {
    ++i;
    num &= (num - 1);
  }
  return i;
#endif
}

static int ctz(int num) {
#ifdef __GNUC__
  return __builtin_ctz(num);
#else
  int i = 0;
  for (; i < 8 * sizeof(int); ++i) {
    if (test_bit(num, i)) {
      return i;
    }
  }
  return i;
#endif
}

template <bool load, Register base_reg>
u32 multiple_load_store(Cpu& cpu, u16 instruction) {
  u32 base = cpu.reg(base_reg);
  u32 cycles = 1;

  // Unspecified behavior for empty list
  const auto reg_list = instruction & 0xff;
  const bool empty_list = reg_list == 0;
  if (load && empty_list) {
    cpu.set_reg(Register::R15, cpu.mmu()->at<u32>(base));
    base += 64;
  } else if (!load && empty_list) {
    cpu.mmu()->set(base, cpu.reg(Register::R15) + 2);
    base += 64;
  } else {
    const auto count = popcount(reg_list);
    const auto first_bit = ctz(reg_list);

    for (int i = first_bit; i < 8; ++i) {
      if (test_bit(instruction, i)) {
        const auto reg = static_cast<Register>(i);
        if constexpr (load) {
          // LDMIA
          cpu.set_reg(reg, cpu.mmu()->at<u32>(base));
        } else {
          // STMIA
          if (reg == base_reg && i != first_bit) {
            cpu.mmu()->set(base, cpu.reg(base_reg) + 4 * count);
          } else {
            cpu.mmu()->set(base, cpu.reg(reg));
          }
        }
        cycles += cpu.mmu()->wait_cycles(
            base, load_store_cycles(load ? reg : base_reg, load));

        base += 4;
      }
    }
  }

  cpu.set_reg(base_reg, base);

  return cycles;
}

u32 conditional_branch(Cpu& cpu, u16 instruction) {
  // The condition is handled externally
  const u32 offset =
      static_cast<u32>(static_cast<s32>(static_cast<s8>(instruction & 0xff)))
      << 1;

  cpu.set_reg(Register::R15, cpu.reg(Register::R15) + offset);
  return 1;
}

u32 software_interrupt(Cpu& cpu, u16 instruction) {
  return execute_software_interrupt(cpu, static_cast<u32>(instruction & 0xff));
}

template <bool is_signed>
u32 unconditional_branch(Cpu& cpu, u16 instruction) {
  const u32 offset = (instruction & 0b111'1111'1111) << 1;
  constexpr u32 mask = /*test_bit(offset, 11) */ is_signed ? 0xfffff800 : 0;
  cpu.set_reg(Register::R15, cpu.reg(Register::R15) + (offset | mask));

  return 3;
}

template <bool part_two>
u32 long_branch_with_link(Cpu& cpu, u16 instruction) {
  const u32 offset = (instruction & 0x7ff);
  const u32 pc = cpu.reg(Register::R15) - 4;
  if constexpr (!part_two) {
    const s32 signed_offset =
        (offset << 12) | (test_bit(offset, 10) ? 0xff800000 : 0);
    cpu.set_reg(Register::R14, pc + signed_offset);
    // cpu.set_reg(Register::R15, pc + 2);
  } else {
    const u32 next_instruction = pc + 2;
    cpu.set_reg(Register::R15, cpu.reg(Register::R14) + (offset << 1) + 4);
    cpu.set_reg(Register::R14, next_instruction | 1);
  }
  return 1;
}

[[noreturn]] u32 invalid_instruction([[maybe_unused]] Cpu& cpu,
                                     [[maybe_unused]] u16 instruction) {
  throw std::runtime_error("invalid instruction");
}

}  // namespace thumb

static constexpr ThumbInstructionTables thumb_lookup_table = [] {
  ThumbInstructionTables instruction_tables;
  auto& res = instruction_tables.interpreter_table;

  const auto decode_thumb_instruction = [&](auto i) {
    constexpr u16 instruction = i << 6;
    const auto prefix = (instruction >> 13) & 0b111;
    switch (prefix) {
      case 0:
        if (((instruction >> 11) & 0b11) == 0b11) {
          constexpr bool immediate_operand = test_bit(instruction, 10);
          constexpr bool subtract = test_bit(instruction, 9);
          constexpr u32 register_or_value = (instruction >> 6) & 0b111;
          res[i] = thumb::add_subtract<immediate_operand, subtract,
                                       register_or_value>;
        } else {
          res[i] =
              thumb::move_shifted_register<static_cast<ShiftType>(
                                               ((instruction >> 11) & 0b11)),
                                           (instruction >> 6) & 0b11111>;
        }
        break;
      case 1: {
        if constexpr (instruction > 0) {
          // Move/compare/add/subtract immediate
          constexpr auto decode_opcode = [](u16 inst) -> Opcode {
            switch ((inst >> 11) & 0b11) {
              case 0:
                return Opcode::Mov;
              case 1:
                return Opcode::Cmp;
              case 2:
                return Opcode::Add;
              case 3:
                return Opcode::Sub;
              default:
                throw "Not reachable";
            }
          };
          constexpr auto opcode = decode_opcode(instruction);
          const auto dest_reg =
              static_cast<Register>((instruction >> 8) & 0b111);

          res[i] = thumb::move_compare_add_subtract_immediate<opcode, dest_reg>;
        }
        break;
      }
      case 2:
        switch ((instruction >> 10) & 0b111111) {
          case 0b010000:
            // ALU operation
            res[i] = thumb::alu_operation<(instruction >> 6) & 0b1111>;
            break;
          case 0b010001:
            // Hi register operation
            res[i] = thumb::hi_register_operation<((instruction >> 8) & 0b11),
                                                  test_bit(instruction, 7),
                                                  test_bit(instruction, 6)>;
            break;
          case 0b010010:
          case 0b010011: {
            //  Load/store
            constexpr auto dest_reg =
                static_cast<Register>((instruction >> 8) & 0b111);
            res[i] = thumb::pc_relative_load<dest_reg>;
            break;
          }
          default: {
            constexpr bool sign_extended = test_bit(instruction, 9);
            constexpr u32 offset_reg = (instruction >> 6) & 0b111;
            if constexpr (sign_extended) {
              constexpr auto opcode = (((instruction >> 10) & 0b1) << 1) |
                                      ((instruction >> 11) & 0b1);

              using Type = std::conditional_t<
                  opcode <= 0b01, u16,
                  std::conditional_t<opcode == 0b10, s8, s16>>;

              constexpr bool load = opcode > 0;
              res[i] = thumb::load_store<load, true, offset_reg, Type>;
            } else {
              constexpr bool load = test_bit(instruction, 11);
              using Type =
                  std::conditional_t<test_bit(instruction, 10), u8, u32>;
              res[i] = thumb::load_store<load, true, offset_reg, Type>;
            }
            break;
          }
        }
        break;
      case 3: {
        constexpr bool load = test_bit(instruction, 11);
        constexpr bool byte_transfer = test_bit(instruction, 12);
        constexpr u32 offset = ((instruction >> 6) & 0b11111)
                               << (byte_transfer ? 0 : 2);
        using Type = std::conditional_t<byte_transfer, u8, u32>;
        res[i] = thumb::load_store<load, false, offset, Type>;

        break;
      }

      case 4: {
        constexpr bool is_sp_relative_store = test_bit(instruction, 12);
        if constexpr (is_sp_relative_store) {
          res[i] = nullptr;
          constexpr bool load = test_bit(instruction, 11);
          constexpr auto dest_reg =
              static_cast<Register>((instruction >> 8) & 0b111);
          res[i] = thumb::sp_relative_load_store<load, dest_reg>;
        } else {
          constexpr u32 offset = ((instruction >> 6) & 0b11111) << 1;
          constexpr bool load = test_bit(instruction, 11);
          res[i] = thumb::load_store<load, false, offset, u16>;
        }
        break;
      }
      case 5: {
        if constexpr (!test_bit(instruction, 12)) {
          constexpr bool sp = test_bit(instruction, 11);
          constexpr auto dest_reg =
              static_cast<Register>((instruction >> 8) & 0b111);
          res[i] = thumb::load_address<sp, dest_reg>;
        } else if constexpr (!test_bit(instruction, 10)) {
          // Add offset to stack pointer

          constexpr bool negative = test_bit(instruction, 7);
          res[i] = thumb::add_offset_to_stack_pointer<negative>;
        } else {
          // Push pop
          constexpr bool load = test_bit(instruction, 11);
          constexpr bool load_pc_or_store_lr = test_bit(instruction, 8);
          res[i] = thumb::push_pop_registers<load, load_pc_or_store_lr>;
        }
        break;
      }
      case 6: {
        if constexpr (!test_bit(instruction, 12)) {
          constexpr bool load = test_bit(instruction, 11);
          constexpr auto base_reg =
              static_cast<Register>((instruction >> 8) & 0b111);
          res[i] = thumb::multiple_load_store<load, base_reg>;

        } else {
          if constexpr (((instruction >> 8) & 0b1111) == 0b1111) {
            // Swi
            res[i] = thumb::software_interrupt;
          } else {
            // Conditional branch
            res[i] = thumb::conditional_branch;
          }
        }
        break;
      }
      case 7: {
        if (test_bit(instruction, 12)) {
          constexpr bool part_two = test_bit(instruction, 11);
          res[i] = thumb::long_branch_with_link<part_two>;
        } else {
          res[i] = thumb::unconditional_branch<test_bit(instruction, 10)>;
        }
        break;
      }
      default:
        res[i] = thumb::invalid_instruction;
        break;
    }
  };
  for_static<2>([&](auto base_index) {
    for_static<512>([&](auto i) {
      constexpr auto real_index = (512 * decltype(base_index)::value) + i;
      decode_thumb_instruction(
          std::integral_constant<std::size_t, real_index>{});
    });
  });
  return instruction_tables;
}();

const ThumbInstructionTables thumb_instruction_tables = thumb_lookup_table;

}  // namespace gb::advance
