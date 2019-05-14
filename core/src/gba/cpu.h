#pragma once
#include <algorithm>
#include <array>
#include <optional>
#include <tuple>
#include "types.h"
#include "utils.h"

namespace gb::advance {
// Inspired by
// https://github.com/lefticus/cpp_box/blob/master/include/cpp_box/arm.hpp

enum class InstructionType {
  DataProcessing,
  MRS,
  MSR,
  Multiply,
  MultiplyLong,
  SingleDataSwap,
  BranchAndExchange,
  HalfwordDataTransferReg,
  HalfwordDataTransferImm,
  SingleDataTransfer,
  Undefined,
  BlockDataTransfer,
  Branch,
  CoprocessorDataTransfer,
  CoprocessorDataOperation,
  CoprocessorRegisterTransfer,
  SoftwareInterrupt,

};

enum class Opcode : u32 {
  And = 0b0000,
  Eor = 0b0001,
  Sub = 0b0010,
  Rsb = 0b0011,
  Add = 0b0100,
  Adc = 0b0101,
  Sbc = 0b0110,
  Rsc = 0b0111,
  Tst = 0b1000,
  Teq = 0b1001,
  Cmp = 0b1010,
  Cmn = 0b1011,
  Orr = 0b1100,
  Mov = 0b1101,
  Bic = 0b1110,
  Mvn = 0b1111,
};

enum class Register : u32 {
  R0 = 0,
  R1,
  R2,
  R3,
  R4,
  R5,
  R6,
  R7,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
};

struct LookupEntry {
  u32 mask = 0;
  u32 expected = 0;
  InstructionType type = InstructionType::Undefined;

  constexpr LookupEntry(InstructionType type_) : type{type_} {}
  constexpr LookupEntry() = default;

  template <typename... Args>
  constexpr LookupEntry& mask_bits(Args... args) {
    mask = set_bits<u32>(args...);
    return *this;
  }
  constexpr LookupEntry& mask_bit_range(int begin, int end) {
    for (int i = begin; i <= end; ++i) {
      mask |= (1 << i);
    }
    return *this;
  }

  template <typename... Args>
  constexpr LookupEntry& expect_bits(Args... args) {
    expected = set_bits<u32>(args...);
    return *this;
  }
};

template <typename T>
class Integer {
  static_assert(std::is_integral_v<T>);

 public:
  constexpr explicit Integer(T value_) : value{value_} {}
  [[nodiscard]] constexpr T data() const { return value; }
  [[nodiscard]] constexpr bool test_bit(unsigned int bit) const {
    return gb::test_bit(value, bit);
  }

  constexpr void set_bit(unsigned int bit, bool set) {
    const T mask = 1 << bit;
    value = (value & ~mask) | (set ? 0 : mask);
  }

 protected:
  T value;
};

class ProgramStatus : public Integer<u32> {
 public:
  constexpr ProgramStatus() : Integer::Integer{0} {}

  [[nodiscard]] constexpr bool negative() const { return test_bit(31); }
  constexpr void set_negative(bool set) { set_bit(31, set); }

  [[nodiscard]] constexpr bool zero() const { return test_bit(30); }
  constexpr void set_zero(bool set) { set_bit(30, set); }

  [[nodiscard]] constexpr bool carry() const { return test_bit(29); }
  constexpr void set_carry(bool set) { set_bit(29, set); }

  [[nodiscard]] constexpr bool overflow() const { return test_bit(28); }
  constexpr void set_overflow(bool set) { set_bit(28, set); }
};

class Instruction : public Integer<u32> {
 public:
  enum class Condition : u32 {
    EQ = 0,  // Z set
    NE,      // Z clear
    CS,      // C set
    CC,      // C clear
    MI,      // N set
    PL,      // N clear
    VS,      // V set
    VC,      // V clear
    HI,      // C set and Z clear
    LS,      // C clear or Z set
    GE,      // N equals V
    LT,      // N not equal to V
    GT,      // Z clear AND (N equals V)
    LE,      // Z set OR (N not equal to V)
    AL,      // ignored
  };

  using Integer::Integer;
  [[nodiscard]] constexpr Register dest_register() const {
    return static_cast<Register>((value >> 12) & 0xf);
  }
  [[nodiscard]] constexpr Register operand_register() const {
    return static_cast<Register>((value >> 16) & 0xf);
  }
  [[nodiscard]] constexpr Condition condition() const {
    return static_cast<Condition>((value >> 28) & 0xf);
  }

  [[nodiscard]] constexpr bool should_execute(
      ProgramStatus program_status) const {
    switch (condition()) {
      case Condition::EQ:
        return program_status.zero();
      case Condition::NE:
        return !program_status.zero();
      case Condition::CS:
        return program_status.carry();
      case Condition::CC:
        return !program_status.carry();
      case Condition::MI:
        return program_status.negative();
      case Condition::PL:
        return !program_status.negative();
      case Condition::VS:
        return program_status.overflow();
      case Condition::VC:
        return !program_status.overflow();
      case Condition::HI:
        return program_status.carry() && !program_status.zero();
      case Condition::LS:
        return !program_status.carry() || program_status.zero();
      case Condition::GE:
        return program_status.negative() == program_status.overflow();
      case Condition::LT:
        return program_status.negative() != program_status.overflow();
      case Condition::GT:
        return !program_status.zero() &&
               program_status.negative() == program_status.overflow();
      case Condition::LE:
        return program_status.zero() ||
               program_status.negative() != program_status.overflow();
      case Condition::AL:
        return true;
    }
  }
};

class Cpu {
 public:
  enum class Mode { User = 0, FIQ, Supervisor, Abort, IRQ, Undefined };
  std::array<u32, 16> m_regs = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  Mode m_mode = Mode::User;

  constexpr u32& reg(Register reg_selected) {
    const u32 index = static_cast<u32>(reg_selected);
    return m_regs[index];
  }

  constexpr u32 reg(Register reg_selected) const {
    const u32 index = static_cast<u32>(reg_selected);
    return m_regs[index];
  }

  [[nodiscard]] constexpr ProgramStatus& program_status() {
    return m_current_program_status;
  }

  [[nodiscard]] constexpr const ProgramStatus& program_status() const {
    return m_current_program_status;
  }

#if 0
  [[nodiscard]] constexpr ProgramStatus saved_program_status() const {
    return m_saved_program_status[static_cast<u32>(m_mode)];
  }
#endif

  constexpr void move_spsr_to_cpsr() {
    if (m_mode != Mode::User) {
      m_current_program_status =
          m_saved_program_status[static_cast<u32>(m_mode) - 1];
    }
  }

  constexpr u32 carry() const { return program_status().carry() ? 1 : 0; }

  friend class DataProcessing;

 private:
  ProgramStatus m_current_program_status{};
  std::array<ProgramStatus, 5> m_saved_program_status{};
};

class DataProcessing : public Instruction {
  enum class ShiftType : u32 {
    LogicalLeft = 0,
    LogicalRight,
    ArithmeticRight,
    RotateRight,
  };

 public:
  struct ShiftResult {
    ShiftType shift_type;
    u32 shift_amount;
    u32 shift_value;
    std::optional<u32> result;
    std::optional<bool> set_carry;
  };
  using Instruction::Instruction;
  [[nodiscard]] constexpr bool immediate_operand() const {
    return test_bit(25);
  }

  [[nodiscard]] constexpr Opcode opcode() const {
    return static_cast<Opcode>((value >> 21) & 0xf);
  }

  [[nodiscard]] constexpr bool set_condition_code() const {
    return test_bit(20);
  }

  [[nodiscard]] constexpr u8 immediate_value() const {
    return static_cast<u8>(value & 0xff);
  }

  [[nodiscard]] constexpr u8 immediate_shift() const {
    return static_cast<u8>((value >> 8) & 0xf);
  }

#if 0
  [[nodiscard]] constexpr u8 register_shift(const Cpu& cpu) const {
    if (test_bit(4)) {
      // Shift by bottom byte of register
      const u32 reg = (value >> 8) & 0xf;
      return reg == 15 ? 0 : static_cast<u8>(cpu.regs.at(reg) & 0xff);
    }

    // Shift by 5 bit number
    return static_cast<u8>((value >> 7) & 0b0001'1111);
  }
#endif

  [[nodiscard]] constexpr ShiftResult shift_value(const Cpu& cpu) const {
    if (immediate_operand()) {
      return {ShiftType::RotateRight,
              static_cast<u8>(((value >> 8) & 0xf) * 2),
              value & 0xff,
              {},
              {}};
    }
    const auto shift_type = static_cast<ShiftType>((value >> 5) & 0b11);
    const auto reg = static_cast<Register>(value & 0xf);
    const bool register_specified = test_bit(4);

    const u8 shift_amount = [&]() -> u8 {
      if (register_specified) {
        // Shift by bottom byte of register
        const u32 reg = (value >> 8) & 0xf;
        const u8 amount =
            reg == 15
                ? 0
                : static_cast<u8>(cpu.reg(static_cast<Register>(reg)) & 0xff);
        return amount;
      }

      // Shift by 5 bit number
      return static_cast<u8>((value >> 7) & 0b0001'1111);
    }();
    const u32 reg_value = cpu.reg(reg);  // Rm

    // TODO: Finish shift carry calculation
    // Allow carry and the result to be overridden
    const auto [error_value, set_carry] =
        [&]() -> std::tuple<std::optional<u32>, std::optional<bool>> {
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
          return {{}, {}};

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
              return {reg_value, gb::test_bit(reg_value, 31)};
            }
          }
          return {{}, {}};
        case ShiftType::ArithmeticRight:
          if (shift_amount == 32) {
            const bool is_negative = gb::test_bit(reg_value, 31);
            return {is_negative ? 0xffffffff : 0, is_negative};
          }
          return {{}, {}};

        case ShiftType::RotateRight:
          if (shift_amount == 0) {
            const bool carry = gb::test_bit(reg_value, 0);
            return {(cpu.carry() << 31) | (reg_value >> 1), carry};
          }
          return {{}, {}};

        default:
          return {{}, {}};
      }
    }();

    return {shift_type, shift_amount, reg_value, error_value, set_carry};
  }

  [[nodiscard]] constexpr std::tuple<bool, u32> compute_operand2(
      const Cpu& cpu) const {
    const auto [shift_type, shift_amount, shift_operand, result, set_carry] =
        shift_value(cpu);
    // const bool carry = set_carry.value_or(cpu.program_status().carry());
    // const bool carry = cpu.program_status().carry();
    constexpr auto compute_carry = [](ShiftType shift_type, u32 shift_operand,
                                      u32 shift_amount) {
      switch (shift_type) {
        case ShiftType::LogicalLeft:
          return gb::test_bit(shift_operand, 32 - shift_amount);
        case ShiftType::LogicalRight:
          return gb::test_bit(shift_operand, shift_amount - 1);
        case ShiftType::ArithmeticRight:
          return gb::test_bit(shift_operand, shift_amount - 1);
        case ShiftType::RotateRight:
          return gb::test_bit(shift_operand, shift_amount - 1);
      }
    };

    constexpr auto compute_result = [](ShiftType shift_type, u32 shift_operand,
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
    };

    return {set_carry.value_or(
                compute_carry(shift_type, shift_operand, shift_amount)),
            result.value_or(
                compute_result(shift_type, shift_operand, shift_amount))};
  }

  constexpr void execute(Cpu& cpu) {
    // TODO: Find out why structured bindings don't work on clang
    const auto operand2_result = compute_operand2(cpu);
    const bool shift_carry = std::get<0>(operand2_result);
    const u32 operand2 = std::get<1>(operand2_result);
    // const auto [shift_carry, operand2] = compute_operand2(cpu);

    const u32 operand1 = cpu.reg(operand_register());

    const u32 carry_value = cpu.carry();

    const Register dest_reg = dest_register();

    const auto write_result = [&cpu, this](Register dest_reg, u32 result) {
      if (dest_reg == Register::R15 && set_condition_code()) {
        cpu.move_spsr_to_cpsr();
      }
      cpu.reg(dest_reg) = result;
    };

    const auto run_logical = [&](u32 result, bool write = true) {
      if (set_condition_code()) {
        cpu.program_status().set_carry(shift_carry);
        cpu.program_status().set_zero(result == 0);
        cpu.program_status().set_negative(gb::test_bit(result, 31));
      }

      if (write) {
        write_result(dest_reg, result);
      }
    };

    const auto run_arithmetic = [&](u64 op1, u64 op2, bool write, auto impl) {
      const u32 dest_value = cpu.reg(dest_register());

      const u64 result = impl(op1, op2);

      if (set_condition_code()) {
        cpu.program_status().set_overflow(gb::test_bit(dest_value, 31) !=
                                          gb::test_bit(result, 31));
        cpu.program_status().set_zero(result == 0);
        cpu.program_status().set_negative(gb::test_bit(result, 31));
        cpu.program_status().set_carry(result >
                                       std::numeric_limits<u32>::max());
      }

      if (write) {
        write_result(dest_reg, static_cast<u32>(result));
      }
    };

    switch (opcode()) {
      // Arithmetic
      case Opcode::Sub:
        run_arithmetic(operand1, operand2, true,
                       [](u64 op1, u64 op2) { return op1 - op2; });
        break;
      case Opcode::Rsb:
        run_arithmetic(operand1, operand2, true,
                       [](u64 op1, u64 op2) { return op2 - op1; });
        break;
      case Opcode::Add:
        run_arithmetic(operand1, operand2, true,
                       [](u64 op1, u64 op2) { return op1 + op2; });
        break;
        // return op1 + op2;
      case Opcode::Adc:
        run_arithmetic(operand1, operand2, true,
                       [carry_value](u64 op1, u64 op2) {
                         return op1 + op2 + carry_value;
                       });
        break;
        // return op1 + op2 + carry_value;
      case Opcode::Sbc:
        run_arithmetic(operand1, operand2, true,
                       [carry_value](u64 op1, u64 op2) {
                         return op1 - op2 + carry_value - 1;
                       });
        break;
        // return op1 - op2 + carry_value - 1;
      case Opcode::Rsc:
        run_arithmetic(operand1, operand2, true,
                       [carry_value](u64 op1, u64 op2) {
                         return op2 - op1 + carry_value - 1;
                       });
        break;
        // return op2 - op1 + carry_value - 1;
      case Opcode::Cmp:
        run_arithmetic(operand1, operand2, false,
                       [](u64 op1, u64 op2) { return op1 - op2; });
        break;
      case Opcode::Cmn:
        run_arithmetic(operand1, operand2, false,
                       [](u64 op1, u64 op2) { return op1 + op2; });
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
};

class MRS : public DataProcessing {
 public:
  constexpr void execute(Cpu& cpu) {}
};

constexpr std::array<LookupEntry, 17> generate_lookup_table() {
  std::array<LookupEntry, 17> lookup_table = {
      LookupEntry{InstructionType::DataProcessing}.mask_bits(27, 26),
      LookupEntry{InstructionType::MRS}
          .mask_bit_range(23, 27)
          .mask_bit_range(16, 21)
          .mask_bit_range(0, 11)
          .expect_bits(24, 19, 18, 17, 16),
      LookupEntry{InstructionType::MSR},
      LookupEntry{InstructionType::Multiply}
          .mask_bit_range(22, 27)
          .mask_bit_range(4, 7)
          .expect_bits(7, 4),
      LookupEntry{InstructionType::MultiplyLong}
          .mask_bit_range(23, 27)
          .mask_bit_range(4, 7)
          .expect_bits(23, 7, 4),
      LookupEntry{InstructionType::SingleDataSwap}
          .mask_bit_range(20, 27)
          .mask_bit_range(4, 11)
          .expect_bits(24, 7, 4),
      LookupEntry{InstructionType::BranchAndExchange}
          .mask_bit_range(4, 27)
          .expect_bits(25, 21, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 4),
      LookupEntry{InstructionType::HalfwordDataTransferReg}
          .mask_bits(27, 26, 25, 22, 11, 10, 9, 8, 7, 4)
          .expect_bits(7, 4),
      LookupEntry{InstructionType::HalfwordDataTransferImm}
          .mask_bits(27, 26, 25, 22, 7, 4)
          .expect_bits(22, 7, 4),
      LookupEntry{InstructionType::SingleDataTransfer}
          .mask_bits(27, 26)
          .expect_bits(26),
      LookupEntry{InstructionType::Undefined}
          .mask_bits(27, 26, 25, 4)
          .expect_bits(26, 25, 4),
      LookupEntry{InstructionType::BlockDataTransfer}
          .mask_bits(27, 26, 25)
          .expect_bits(27),
      LookupEntry{InstructionType::Branch}
          .mask_bits(27, 26, 25)
          .expect_bits(27, 25),
      LookupEntry{InstructionType::CoprocessorDataTransfer}
          .mask_bits(27, 26, 25)
          .expect_bits(27, 26),
      LookupEntry{InstructionType::CoprocessorDataOperation}
          .mask_bits(27, 26, 25, 24, 4)
          .expect_bits(27, 26, 25),
      LookupEntry{InstructionType::CoprocessorRegisterTransfer}
          .mask_bits(27, 26, 25, 4)
          .expect_bits(27, 26, 25, 4),
      LookupEntry{InstructionType::SoftwareInterrupt}
          .mask_bits(27, 26, 25, 24)
          .expect_bits(27, 26, 25, 24)};
  return lookup_table;
}

constexpr std::array<LookupEntry, 17> lookup_table = generate_lookup_table();

constexpr auto decode_instruction_type(u32 instruction) {
  const auto decoded_type =
      constexpr_find(lookup_table.begin(), lookup_table.end(),
                     [instruction](const LookupEntry& entry) {
                       return (instruction & entry.mask) == entry.expected;
                     });

  return decoded_type == lookup_table.end() ? InstructionType::Undefined
                                            : decoded_type->type;
}
namespace tests::cpu {
static_assert([]() constexpr->bool {
  // mov r0, #0
  return decode_instruction_type(0xe3a00000) == InstructionType::DataProcessing;
}());
static_assert([]() -> bool {
  Cpu cpu;
  // mov r0, #456
  DataProcessing inst{0xe3a00f72};
  if (inst.dest_register() != Register::R0) {
    throw "dest should be r0";
  }
  if (!inst.immediate_operand()) {
    throw "it should be immediate";
  }

  inst.execute(cpu);

  return cpu.reg(Register::R0) == 456;
}());
static_assert(
    decode_instruction_type(0b0000'0011'1111'0011'0011'0000'0000'0000) ==
    InstructionType::DataProcessing);
}  // namespace tests::cpu

}  // namespace gb::advance
