#pragma once
#include "cpu.h"

namespace gb::advance {
template <typename T>
void run_load(Cpu& cpu, u32 addr, Register dest_reg) {
  constexpr auto mask = sizeof(T) - 1;
  const auto aligned_addr = addr & ~mask;
  const auto rotate_amount = (addr & mask) * 8;

  const auto value = [&] {
    if constexpr (std::is_same_v<T, s16>) {
      if ((addr & mask) != 0) {
        return static_cast<s16>(cpu.mmu()->at<s8>(addr));
      }
    }
    return cpu.mmu()->at<T>(aligned_addr);
  }();
  cpu.set_reg(dest_reg,
              static_cast<std::conditional_t<std::is_signed_v<T>, s32, u32>>(
                  rotate_right(value, rotate_amount)));
}

constexpr Cycles load_store_cycles(Register dest_reg, bool load) {
  if (!load) {
    return 2_nonseq;
  }

  // Load
  return dest_reg == Register::R15 ? (2_seq + 2_nonseq + 1_intern)
                                   : (1_seq + 1_nonseq + 1_intern);
}
namespace multiply::detail {

[[nodiscard]] constexpr Cycles multiply_cycles(u32 rhs_operand,
                                               bool accumulate) {
  const u32 multiply_cycles = [&]() -> u32 {
    if (const u32 masked = rhs_operand & 0xffffff00;
        masked == 0 || masked == 0xffffff00) {
      return 1;
    }
    if (const u32 masked = rhs_operand & 0xffff0000;
        masked == 0 || masked == 0xffff0000) {
      return 2;
    }

    if (const u32 masked = rhs_operand & 0xff000000;
        masked == 0 || masked == 0xff000000) {
      return 3;
    }

    return 4;
  }();

  return Cycles{1, 0, multiply_cycles + (accumulate ? 1 : 0)};
}
[[nodiscard]] constexpr Cycles multiply_long_cycles(u32 rhs_operand,
                                                    bool accumulate,
                                                    bool is_signed) {
  if (is_signed) {
    return multiply_cycles(rhs_operand, accumulate);
  }

  const u32 multiply_cycles = [rhs_operand] {
    if (const u32 masked = rhs_operand & 0xffffff00; masked == 0) {
      return 1;
    }

    if (const u32 masked = rhs_operand & 0xffff0000; masked == 0) {
      return 2;
    }

    if (const u32 masked = rhs_operand & 0xff000000; masked == 0) {
      return 3;
    }

    return 4;
  }();

  return {1, 0, multiply_cycles + (accumulate ? 2 : 1)};
}
template <typename T>
[[nodiscard]] constexpr T multiply(T lhs, T rhs) {
  static_assert(std::is_integral_v<T>);

  return lhs * rhs;
}

}  // namespace multiply::detail
enum class ShiftType : u32 {
  LogicalLeft = 0,
  LogicalRight,
  ArithmeticRight,
  RotateRight,
};

struct ShiftResult {
  ShiftType shift_type;
  u32 shift_amount;
  u32 shift_value;
  std::optional<u32> result;
  std::optional<bool> set_carry;
};

constexpr ShiftResult compute_shift_value(u32 value, const Cpu& cpu) {
  const auto shift_type = static_cast<ShiftType>((value >> 5) & 0b11);
  const auto reg = static_cast<Register>(value & 0xf);
  const bool register_specified =
      gb::test_bit(value, 4);  // Shift is specified by a register

  const u8 shift_amount = [&]() -> u8 {
    if (register_specified) {
      // Shift by bottom byte of register
      const u32 shift_reg = (value >> 8) & 0xf;
      const u8 amount =
          shift_reg == 15
              ? 0
              : static_cast<u8>(cpu.reg(static_cast<Register>(shift_reg)) &
                                0xff);
      return amount;
    }

    // Shift by 5 bit number
    return static_cast<u8>((value >> 7) & 0b0001'1111);
  }();

  const u32 reg_value =
      cpu.reg(reg) +
      // TODO: Is this offset correct?
      (register_specified && reg == Register::R15 ? 4 : 0);  // Rm

  if (shift_amount > 0 && shift_amount < 32) {
    return {shift_type, shift_amount, reg_value, std::nullopt, std::nullopt};
  }
  // Allow carry and the result to be overridden
  const auto [error_value, set_carry] =
      [&]() -> std::pair<std::optional<u32>, std::optional<bool>> {
    if (register_specified && shift_amount == 0) {
      return {{}, cpu.program_status().carry()};
    }
    switch (shift_type) {
      case ShiftType::LogicalLeft:
        if (register_specified) {
          if (shift_amount == 32) {
            return {0, gb::test_bit(reg_value, 0)};
          }
          if (shift_amount > 32) {
            return {0, false};
          }
        } else {
          if (shift_amount == 0) {
            // return {0, gb::test_bit(reg_value, 31)};
            return {reg_value, cpu.program_status().carry()};
          }
        }
        return {std::nullopt, std::nullopt};

      case ShiftType::LogicalRight:
        if (register_specified) {
          if (shift_amount == 32) {
            return {0, gb::test_bit(reg_value, 31)};
          }
          if (shift_amount > 32) {
            return {0, false};
          }
        } else {
          if (shift_amount == 0) {
            return {0, gb::test_bit(reg_value, 31)};
            // return {reg_value, gb::test_bit(reg_value, 31)};
          }
        }
        return {std::nullopt, std::nullopt};
      case ShiftType::ArithmeticRight:
        if (shift_amount >= 32 || shift_amount == 0) {
          const bool is_negative = gb::test_bit(reg_value, 31);
          return {is_negative ? 0xffffffff : 0, is_negative};
        }
        return {std::nullopt, std::nullopt};

      case ShiftType::RotateRight:
        if (shift_amount == 0) {
          const bool carry = gb::test_bit(reg_value, 0);
          return {(cpu.carry() << 31) | (reg_value >> 1), carry};
        }
        return {std::nullopt, std::nullopt};

      default:
        return {std::nullopt, std::nullopt};
    }
  }();

  return {shift_type, shift_amount, reg_value, error_value, set_carry};
}

constexpr bool compute_carry(ShiftType shift_type,
                             u32 shift_operand,
                             u32 shift_amount) {
  if (shift_amount == 0) {
    return false;
  }
  switch (shift_type) {
    case ShiftType::LogicalLeft:
      if (shift_amount > 31) {
        return false;
      }
      return shift_amount != 0 &&
             gb::test_bit(shift_operand, 32 - shift_amount);
    case ShiftType::LogicalRight:
      if (shift_amount > 31) {
        return test_bit(shift_operand, 31);
      }
      return gb::test_bit(shift_operand, shift_amount - 1);
    case ShiftType::ArithmeticRight:
      if (shift_amount > 31) {
        return test_bit(shift_operand, 31);
      }
      return gb::test_bit(shift_operand, shift_amount - 1);
    case ShiftType::RotateRight:
      if (shift_amount > 32) {
        return test_bit(shift_operand, shift_amount - 32 - 1);
      }
      return shift_amount != 0 && gb::test_bit(shift_operand, shift_amount - 1);
  }
  GB_UNREACHABLE();
}

struct ShiftFlags {
  std::optional<u32> result;
  bool set_carry;
};

inline ShiftFlags compute_carry(Cpu& cpu,
                                ShiftType shift_type,
                                bool register_specified,
                                u32 reg_value,
                                u32 shift_amount) {
  const auto [error_value, set_carry] =
      [&]() -> std::pair<std::optional<u32>, std::optional<bool>> {
    if (register_specified && shift_amount == 0) {
      return {{}, cpu.program_status().carry()};
    }
    switch (shift_type) {
      case ShiftType::LogicalLeft:
        if (register_specified) {
          if (shift_amount == 32) {
            return {0, gb::test_bit(reg_value, 0)};
          }
          if (shift_amount > 32) {
            return {0, false};
          }
        } else {
          if (shift_amount == 0) {
            return {reg_value, cpu.program_status().carry()};
          }
        }
        return {std::nullopt, std::nullopt};

      case ShiftType::LogicalRight:
        if (register_specified) {
          if (shift_amount == 32) {
            return {0, gb::test_bit(reg_value, 31)};
          }
          if (shift_amount > 32) {
            return {0, false};
          }
        } else {
          if (shift_amount == 0) {
            return {0, gb::test_bit(reg_value, 31)};
          }
        }
        return {std::nullopt, std::nullopt};
      case ShiftType::ArithmeticRight:
        if (shift_amount >= 32 || shift_amount == 0) {
          const bool is_negative = gb::test_bit(reg_value, 31);
          return {is_negative ? 0xffffffff : 0, is_negative};
        }
        return {std::nullopt, std::nullopt};

      case ShiftType::RotateRight:
        if (shift_amount == 0) {
          const bool carry = gb::test_bit(reg_value, 0);
          return {(cpu.carry() << 31) | (reg_value >> 1), carry};
        }
        return {std::nullopt, std::nullopt};

      default:
        return {std::nullopt, std::nullopt};
    }
  }();
  return {error_value,
          set_carry.value_or(!set_carry.has_value() && [&]() -> bool {
            switch (shift_type) {
              case ShiftType::LogicalLeft:
                return shift_amount != 0 &&
                       gb::test_bit(reg_value, 32 - shift_amount);
              case ShiftType::LogicalRight:
                return gb::test_bit(reg_value, shift_amount - 1);
              case ShiftType::ArithmeticRight:
                return gb::test_bit(reg_value, shift_amount - 1);
              case ShiftType::RotateRight:
                return gb::test_bit(
                    reg_value,
                    (shift_amount > 32 ? shift_amount - 32 : shift_amount) - 1);
            }
            GB_UNREACHABLE();
          }())};
}

constexpr u32 compute_result(ShiftType shift_type,
                             u32 shift_operand,
                             u32 shift_amount) {
  switch (shift_type) {
    case ShiftType::LogicalLeft:
      return shift_operand << shift_amount;
    case ShiftType::LogicalRight:
      return shift_operand >> shift_amount;
    case ShiftType::ArithmeticRight:
      return arithmetic_shift_right(shift_operand, shift_amount);
    case ShiftType::RotateRight:
      return rotate_right(shift_operand, shift_amount);
  }
  GB_UNREACHABLE();
}

constexpr std::tuple<bool, u32> compute_shifted_operand(
    const ShiftResult shift_result) {
  const auto [shift_type, shift_amount, shift_operand, result, set_carry] =
      shift_result;

  return {set_carry.value_or(
              compute_carry(shift_type, shift_operand, shift_amount)),
          result.value_or(
              !result.has_value()
                  ? compute_result(shift_type, shift_operand, shift_amount)
                  : 0xdeadbeef)};
}

namespace common {
template <Opcode opcode, bool SetConditionCode>
void data_processing(Cpu& cpu,
                     Register dest_reg,
                     u32 operand1,
                     u32 operand2,
                     bool shift_carry) {
  const auto write_result = [&cpu](Register dest, u32 result) {
    if (dest == Register::R15 && SetConditionCode) {
      cpu.move_spsr_to_cpsr();
    }
    cpu.set_reg(dest, result);
  };

  const auto run_logical = [&, shift_carry](u32 result, bool write = true) {
    if constexpr (SetConditionCode) {
      cpu.set_carry(shift_carry);
      cpu.set_zero(result == 0);
      cpu.set_negative(gb::test_bit(result, 31));
    } else {
      static_cast<void>(shift_carry);
    }
    if (write) {
      write_result(dest_reg, result);
    }
  };

  const auto run_arithmetic = [&](u64 op1, u64 op2, bool write, auto impl,
                                  bool invert_carry,
                                  bool carry_override = false,
                                  bool carry_override_value = false) {
    const u64 result = impl(op1, op2);

    const u32 result_32 = static_cast<u32>(result & 0xffffffff);
    if constexpr (SetConditionCode) {
      const bool overflow =
          invert_carry
              ? (((op1 ^ op2) >> 31) & 0b1) && (((op1 ^ result_32) >> 31) & 0b1)
              : (!(((op1 ^ op2) >> 31) & 0b1)) &&
                    (((op2 ^ result_32) >> 31) & 0b1);
      cpu.set_overflow(overflow);
      cpu.set_zero(result_32 == 0);
      cpu.set_negative(gb::test_bit(result_32, 31));
      if (carry_override) {
        cpu.set_carry(carry_override_value);
      } else {
        cpu.set_carry((invert_carry ? (op2 <= op1) : gb::test_bit(result, 32)));
      }
    } else {
      static_cast<void>(carry_override_value);
    }

    if (write) {
      write_result(dest_reg, result_32);
    }
  };
  const u32 carry_value = cpu.carry();

  switch (opcode) {
    // Arithmetic
    case Opcode::Sub:
      run_arithmetic(
          operand1, operand2, true, [](u64 op1, u64 op2) { return op1 - op2; },
          true);
      break;
    case Opcode::Rsb:
      // op2 - op1
      run_arithmetic(
          operand2, operand1, true, [](u64 op1, u64 op2) { return op1 - op2; },
          true);
      break;
    case Opcode::Add:
      run_arithmetic(
          operand1, operand2, true, [](u64 op1, u64 op2) { return op1 + op2; },
          false);
      break;
    case Opcode::Adc:
      run_arithmetic(
          operand1, operand2, true,
          [carry_value](u64 op1, u64 op2) { return op1 + op2 + carry_value; },
          false);
      break;
    case Opcode::Sbc:
      run_arithmetic(
          operand1, operand2, true,
          [carry_value](u64 op1, u64 op2) {
            return op1 - op2 + carry_value - 1;
          },
          true, true,
          (static_cast<u64>(operand2) - static_cast<u64>(carry_value) + 1) <=
              operand1);
      break;
    case Opcode::Rsc:
      run_arithmetic(
          operand1, operand2, true,
          [carry_value](u64 op1, u64 op2) {
            return op2 - op1 + carry_value - 1;
          },
          true, true,
          static_cast<u64>(operand1) + static_cast<u64>(carry_value) - 1 <=
              operand2);
      break;
    case Opcode::Cmp:
      run_arithmetic(
          operand1, operand2, false, [](u64 op1, u64 op2) { return op1 - op2; },
          true);
      break;
    case Opcode::Cmn:
      run_arithmetic(
          operand1, operand2, false, [](u64 op1, u64 op2) { return op1 + op2; },
          false);
      break;
    // Logical
    case Opcode::And:
      run_logical(operand1 & operand2);
      break;
    case Opcode::Eor:
      run_logical(operand1 ^ operand2);
      break;
    case Opcode::Tst:
      run_logical(operand1 & operand2, false);
      break;
    case Opcode::Teq:
      run_logical(operand1 ^ operand2, false);
      break;
    case Opcode::Orr:
      run_logical(operand1 | operand2);
      break;
    case Opcode::Mov:
      run_logical(operand2);
      break;
    case Opcode::Bic:
      run_logical(operand1 & ~operand2);
      break;
    case Opcode::Mvn:
      run_logical(~operand2);
      break;
  }
}
inline void run_shift(Cpu& cpu,
                      ShiftType shift_type,
                      Register dest_reg,
                      u32 value,
                      u32 shift_amount,
                      bool register_specified) {
  const u32 shifted_value = [&]() -> u32 {
    switch (shift_type) {
      // LSL
      case ShiftType::LogicalLeft:
        return shift_amount < 32 ? (value << shift_amount) : 0;
      case ShiftType::LogicalRight:
        return value >> shift_amount;
      case ShiftType::ArithmeticRight:
        return static_cast<s32>(value) >> shift_amount;
      case ShiftType::RotateRight:
        return rotate_right(value, shift_amount);
      default:
        GB_UNREACHABLE();
    }
  }();

  const auto [override_result, carry] =
      compute_carry(cpu, shift_type, register_specified, value, shift_amount);

  cpu.set_negative(test_bit(shifted_value, 31));
  cpu.set_zero(override_result.value_or(shifted_value) == 0);
  cpu.set_carry(carry);

  cpu.set_reg(dest_reg, override_result.value_or(shifted_value));
}

inline void multiply(
    Cpu& cpu,
    Register dest_register,
    Register lhs_register,
    Register rhs_register,
    bool set_condition_code,
    std::optional<Register> accumulate_register = std::nullopt) {
  const u32 rhs_operand = cpu.reg(rhs_register);
  const u32 res = static_cast<u32>(
      static_cast<u64>(cpu.reg(lhs_register)) * static_cast<u64>(rhs_operand) +
      (accumulate_register ? cpu.reg(*accumulate_register) : 0));

  cpu.set_reg(dest_register, res);

  if (set_condition_code) {
    cpu.set_zero(res == 0);
    cpu.set_negative(gb::test_bit(res, 31));
  }
}

inline void branch_and_exchange(Cpu& cpu, Register next_pc_reg) {
  const u32 reg_value = cpu.reg(next_pc_reg);

  const bool thumb_mode = gb::test_bit(reg_value, 0);
  cpu.set_thumb(thumb_mode);

  cpu.set_reg(Register::R15, reg_value);
}
}  // namespace common

}  // namespace gb::advance
