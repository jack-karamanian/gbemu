#pragma once
#include <doctest/doctest.h>
#include <algorithm>
#include <array>
#include <nonstd/span.hpp>
#include <optional>
#include <tuple>
#include "mmu.h"
#include "types.h"
#include "utils.h"

namespace gb::advance {
// Inspired by
// https://github.com/lefticus/cpp_box/blob/master/include/cpp_box/arm.hpp

enum class InstructionType {
  DataProcessing = 0,
  Mrs,
  Msr,
  MsrFlagBits,
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
  static constexpr Integer<T> from_bytes(
      nonstd::span<const u8, sizeof(T)> bytes) {
    return Integer<T>{convert_bytes_endian<T>(bytes)};
  }

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
  using Integer::Integer;
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

constexpr std::array<LookupEntry, 18> generate_lookup_table() {
  constexpr auto swap = [](auto a, auto b) {
    auto tmp = *a;
    *a = *b;
    *b = tmp;
  };
  std::array<LookupEntry, 18> lookup_table = {
      LookupEntry{InstructionType::Multiply}
          .mask_bit_range(22, 27)
          .mask_bit_range(4, 7)
          .expect_bits(7, 4),
      LookupEntry{InstructionType::Mrs}
          .mask_bit_range(23, 27)
          .mask_bit_range(16, 21)
          .mask_bit_range(0, 11)
          .expect_bits(24, 19, 18, 17, 16),
      LookupEntry{InstructionType::Msr}
          .mask_bit_range(23, 27)
          .mask_bit_range(4, 21)
          .expect_bits(24, 21, 19, 16, 15, 14, 13, 12),
      LookupEntry{InstructionType::MsrFlagBits}
          .mask_bits(27, 26, 24, 23)
          .mask_bit_range(12, 21)
          .expect_bits(24, 21, 19, 15, 14, 13, 12),
      LookupEntry{InstructionType::DataProcessing}.mask_bits(27, 26),
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
          .expect_bits(24, 21, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 4),
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

  for (auto i = lookup_table.begin(); i != lookup_table.end(); ++i) {
    swap(i, std::min_element(
                i, lookup_table.end(),
                [](LookupEntry a, LookupEntry b) { return b.mask < a.mask; }));
  }
  return lookup_table;
}

constexpr std::array<LookupEntry, 18> lookup_table = generate_lookup_table();

constexpr auto decode_instruction_type(u32 instruction) {
  const auto decoded_type =
      constexpr_find(lookup_table.begin(), lookup_table.end(),
                     [instruction](const LookupEntry& entry) {
                       return (instruction & entry.mask) == entry.expected;
                     });

  return decoded_type == lookup_table.end() ? InstructionType::Undefined
                                            : decoded_type->type;
}

class Cpu {
 public:
  enum class Mode { User = 0, FIQ, Supervisor, Abort, IRQ, Undefined };
  std::array<u32, 16> m_regs = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  Cpu() = default;
  Cpu(Mmu& mmu) : m_mmu{&mmu} {}

  [[nodiscard]] constexpr u32 reg(Register reg_selected) const {
    const u32 index = static_cast<u32>(reg_selected);

    if (reg_selected == Register::R15) {
      return (m_regs[index] + 4) & ~0b11;
    }
    return m_regs[index];
  }

  constexpr void set_reg(Register reg_selected, u32 value) {
    const u32 index = static_cast<u32>(reg_selected);
    if (reg_selected == Register::R15) {
      value &= ~0b11;
    }
    m_regs[index] = value;
  }

  [[nodiscard]] constexpr ProgramStatus program_status() const {
    return m_current_program_status;
  }

  constexpr void set_program_status(ProgramStatus status) {
    m_current_program_status = status;
  }

  [[nodiscard]] constexpr ProgramStatus saved_program_status() const {
    assert_in_user_mode();
    return m_saved_program_status[index_from_mode()];
  }

  constexpr void set_saved_program_status(ProgramStatus program_status) {
    assert_in_user_mode();
    m_saved_program_status[index_from_mode()] = program_status;
  }

  constexpr void move_spsr_to_cpsr() {
    if (m_mode != Mode::User) {
      m_current_program_status =
          m_saved_program_status[static_cast<u32>(m_mode) - 1];
    }
  }

  constexpr u32 carry() const { return program_status().carry() ? 1 : 0; }

  constexpr void set_carry(bool set) {
    get_current_program_status().set_carry(set);
  }
  constexpr void set_overflow(bool set) {
    get_current_program_status().set_overflow(set);
  }
  constexpr void set_negative(bool set) {
    get_current_program_status().set_negative(set);
  }
  constexpr void set_zero(bool set) {
    get_current_program_status().set_zero(set);
  }

  template <typename T>
  constexpr void run_instruction(T instruction) {
    if (instruction.should_execute(get_current_program_status())) {
      instruction.execute(*this);
    }
  }

  inline void execute();

  friend class DataProcessing;
  friend class SingleDataTransfer;
  friend class HalfwordDataTransfer;
  friend class BlockDataTransfer;

 private:
  constexpr ProgramStatus& get_current_program_status() {
    return m_current_program_status;
  }
  constexpr void assert_in_user_mode() const {
    if (m_mode == Mode::User) {
      throw std::runtime_error(
          "saved program status can not be accessed in user mode");
    }
  }

  [[nodiscard]] constexpr u32 index_from_mode() const {
    return static_cast<u32>(m_mode) - 1;
  }

  Mmu* m_mmu;
  Mode m_mode = Mode::User;
  ProgramStatus m_current_program_status{};
  std::array<ProgramStatus, 5> m_saved_program_status{};
};

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

constexpr std::tuple<bool, u32> compute_shifted_operand(
    const ShiftResult& shift_result) {
  const auto [shift_type, shift_amount, shift_operand, result, set_carry] =
      shift_result;
  // const bool carry = set_carry.value_or(cpu.program_status().carry());
  // const bool carry = cpu.program_status().carry();
  constexpr auto compute_carry = [](ShiftType shift_type, u32 shift_operand,
                                    u32 shift_amount) {
    switch (shift_type) {
      case ShiftType::LogicalLeft:
        return gb::test_bit(shift_operand, 31 - shift_amount);
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

  return {
      set_carry.value_or(
          compute_carry(shift_type, shift_operand, shift_amount)),
      result.value_or(compute_result(shift_type, shift_operand, shift_amount))};
}

class DataProcessing : public Instruction {
 public:
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
    return compute_shift_value(value, cpu);
  }

  [[nodiscard]] constexpr std::tuple<bool, u32> compute_operand2(
      const Cpu& cpu) const {
    const ShiftResult shift_result = shift_value(cpu);
    return compute_shifted_operand(shift_result);
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
      cpu.set_reg(dest_reg, result);
    };

    const auto run_logical = [&](u32 result, bool write = true) {
      if (set_condition_code()) {
        cpu.set_carry(shift_carry);
        cpu.set_zero(result == 0);
        cpu.set_negative(gb::test_bit(result, 31));
      }

      if (write) {
        write_result(dest_reg, result);
      }
    };

    const auto run_arithmetic = [&](u64 op1, u64 op2, bool write, auto impl) {
      const u32 dest_value = cpu.reg(dest_register());

      const u64 result = impl(op1, op2);

      if (set_condition_code()) {
        cpu.set_overflow(gb::test_bit(dest_value, 31) !=
                         gb::test_bit(result, 31));
        cpu.set_zero(result == 0);
        cpu.set_negative(gb::test_bit(result, 31));
        cpu.set_carry(result > std::numeric_limits<u32>::max());
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

class Mrs : public Instruction {
 public:
  using Instruction::Instruction;

  constexpr void execute(Cpu& cpu) {
    const u32 program_status =
        (test_bit(22) ? cpu.saved_program_status() : cpu.program_status())
            .data();

    cpu.set_reg(dest_register(), program_status);
  }
};

class Msr : public Instruction {
 public:
  using Instruction::Instruction;

  constexpr void execute(Cpu& cpu) {
    const ProgramStatus program_status{
        cpu.reg(static_cast<Register>(value & 0xf))};
    if (test_bit(22)) {
      cpu.set_saved_program_status(program_status);
    } else {
      cpu.set_program_status(program_status);
    }
  }
};

class MsrFlagBits : public DataProcessing {
 public:
  using DataProcessing::DataProcessing;

  constexpr void execute(Cpu& cpu) {
    const auto [shift_carry, operand2] = compute_operand2(cpu);

    const u32 masked_program_status =
        (test_bit(22) ? cpu.saved_program_status().data()
                      : cpu.program_status().data()) &
        (~(0xf << 28));

    const ProgramStatus next_program_status{operand2 | masked_program_status};
    if (test_bit(22)) {
      cpu.set_saved_program_status(next_program_status);
    } else {
      cpu.set_program_status(next_program_status);
    }
  }
};

class Multiply : public Instruction {
 public:
  using Instruction::Instruction;
  [[nodiscard]] constexpr Register dest_register() const {
    return static_cast<Register>((value >> 16) & 0xf);
  }

  [[nodiscard]] constexpr Register lhs_register() const {
    return static_cast<Register>(value & 0xf);
  }

  [[nodiscard]] constexpr Register rhs_register() const {
    return static_cast<Register>((value >> 8) & 0xf);
  }

  [[nodiscard]] constexpr Register accumulate_register() const {
    return static_cast<Register>((value >> 12) & 0xf);
  }

  [[nodiscard]] constexpr bool set_condition_code() const {
    return test_bit(20);
  }

  [[nodiscard]] constexpr bool accumulate() const { return test_bit(21); }

  constexpr void execute(Cpu& cpu) {
    const u32 res =
        static_cast<u32>(static_cast<u64>(cpu.reg(lhs_register())) *
                             static_cast<u64>(cpu.reg(rhs_register())) +
                         (accumulate() ? cpu.reg(accumulate_register()) : 0));

    cpu.set_reg(dest_register(), res);

    if (set_condition_code()) {
      cpu.set_zero(res == 0);
      cpu.set_negative(gb::test_bit(res, 31));
    }
  }
};

class MultiplyLong : public Multiply {
 public:
  using Multiply::Multiply;

  [[nodiscard]] constexpr Register dest_register_high() const {
    return static_cast<Register>((value >> 16) & 0xf);
  }

  [[nodiscard]] constexpr Register dest_register_low() const {
    return static_cast<Register>((value >> 12) & 0xf);
  }

  [[nodiscard]] constexpr bool is_signed() const { return test_bit(22); }

  [[nodiscard]] constexpr u64 accumulate_value(const Cpu& cpu) const {
    if (!accumulate()) {
      return 0;
    }

    return (static_cast<u64>(cpu.reg(dest_register_high())) << 32) |
           static_cast<u64>(cpu.reg(dest_register_low()));
  }

  template <typename T>
  [[nodiscard]] constexpr T multiply(T lhs, T rhs) {
    static_assert(std::is_integral_v<T>);

    return lhs * rhs;
  }

  constexpr void execute(Cpu& cpu) {
    const u32 lhs = cpu.reg(lhs_register());
    const u32 rhs = cpu.reg(rhs_register());

    const u64 res =
        (is_signed() ? multiply<s64>(lhs, rhs) : multiply<u64>(lhs, rhs)) +
        accumulate_value(cpu);

    cpu.set_reg(dest_register_high(), (res & 0xffffffff00000000) >> 32);
    cpu.set_reg(dest_register_low(), res & 0xffffffff);

    if (set_condition_code()) {
      cpu.set_zero(res == 0);
      cpu.set_negative(gb::test_bit(res, 63));
    }
  }
};

class Branch : public Instruction {
 public:
  using Instruction::Instruction;
  void execute(Cpu& cpu) {
    const bool link = test_bit(24);
    const bool negative = test_bit(23);

    // Convert 24 bit signed to 32 bit signed
    const s32 offset = ((value & 0b0111'1111'1111'1111'1111'1111) << 2) |
                       (negative ? (0xfe << 24) : 0);

    printf("offset: %d\n", offset);
    const u32 next_pc = cpu.reg(Register::R15) + offset;
    // const u32 next_pc = cpu.reg(Register::R15) + offset - 8;
    printf("next pc: %d\n", next_pc);
    if (link) {
      cpu.set_reg(Register::R14, cpu.reg(Register::R15) - 4);
      // cpu.set_reg(Register::R14, cpu.reg(Register::R15));
    }
    cpu.set_reg(Register::R15, next_pc);
  }
};

class BranchAndExchange : public Instruction {
 public:
  using Instruction::Instruction;

  void execute(Cpu& cpu) {
    const auto next_pc_reg = static_cast<Register>(value & 0xf);
    cpu.set_reg(Register::R15, cpu.reg(next_pc_reg));
  }
};

class SingleDataTransfer : public Instruction {
 public:
  using Instruction::Instruction;

  [[nodiscard]] constexpr bool immediate_offset() const {
    return !test_bit(25);
  }
  [[nodiscard]] constexpr bool preindex() const { return test_bit(24); }
  [[nodiscard]] constexpr bool add_offset_to_base() const {
    return test_bit(23);
  }
  [[nodiscard]] constexpr bool word() const { return !test_bit(22); }
  [[nodiscard]] constexpr bool write_back() const { return test_bit(21); }
  [[nodiscard]] constexpr bool load() const { return test_bit(20); }


  constexpr void run_write_back(Cpu& cpu, u32 addr) {
    const Register base_register = operand_register();
    cpu.set_reg(base_register, addr);
  }

  [[nodiscard]] constexpr u32 calculate_addr(u32 base_value, u32 offset) const {
    const u32 addr =
        add_offset_to_base() ? offset + base_value : offset - base_value;
    return addr;
  }

  [[nodiscard]] constexpr u32 select_addr(Cpu& cpu,
                                          u32 base_value,
                                          u32 offset) {
    if (preindex()) {
      const u32 addr = calculate_addr(base_value, offset);

      if (write_back()) {
        run_write_back(cpu, addr);
      }
      return addr;
    }
    return base_value;
  }

  void execute(Cpu& cpu) {
    Mmu& mmu = *cpu.m_mmu;
    const auto calculate_offset = [&]() -> u32 {
      if (immediate_offset()) {
        return value & 0xfff;
      }

      const ShiftResult shift_result = compute_shift_value(value, cpu);
      const auto [set_carry, offset] = compute_shifted_operand(shift_result);

      return offset;
    };

    const Register base_register = operand_register();
    const u32 base_value = cpu.reg(base_register);

    const u32 offset = calculate_offset();

    // The address to be used in the transfer
    const auto [aligned_addr, raw_addr,
                rotate_amount] = [&]() -> std::tuple<u32, u32, u32> {
      const u32 raw_addr = select_addr(cpu, base_value, offset);
      return {raw_addr & ~0b11, raw_addr, (raw_addr & 0b11) * 8};
    }();

    if (load()) {
      if (word()) {
        // Load a word
        const u32 value =
            rotate_right(mmu.at<u32>(aligned_addr & ~0b11), rotate_amount);
        printf("aligned_addr: %d\n", aligned_addr);

        cpu.set_reg(dest_register(), value);

      } else {
        // Load a byte
        const u8 value = mmu.at<u8>(raw_addr);
        cpu.set_reg(dest_register(), value);
      }
    } else {
      const u32 stored_value = cpu.reg(dest_register());
      if (word()) {
        // Store a word
        mmu.set(aligned_addr, stored_value);
      } else {
        // Store a byte
        mmu.set(raw_addr, static_cast<u8>(stored_value & 0xff));
      }
    }

    if (!preindex()) {
      const u32 writeback_addr = select_addr(cpu, base_value, offset);
      run_write_back(cpu, writeback_addr);
    }
  }
};

class HalfwordDataTransfer : public SingleDataTransfer {
 public:
  enum class TransferType : u32 {
    Swp = 0,
    UnsignedHalfword,
    SignedByte,
    SignedHalfword,
  };

  using SingleDataTransfer::SingleDataTransfer;

  [[nodiscard]] constexpr bool immediate_offset() const { return test_bit(22); }

  [[nodiscard]] constexpr u32 offset_value(const Cpu& cpu) const {
    if (immediate_offset()) {
      return ((value & 0xf00) >> 4) | (value & 0xf);
    }

    const auto offset_reg = static_cast<Register>(value & 0xf);
    return cpu.reg(offset_reg);
  }

  template <typename T>
  constexpr void run_load(Cpu& cpu, u32 addr) {
    const Register dest_reg = dest_register();

    printf("addr: %d\n", addr);
    if constexpr (std::is_signed_v<T>) {
      cpu.set_reg(dest_reg, static_cast<s32>(cpu.m_mmu->at<T>(addr)));
    } else {
      cpu.set_reg(dest_reg, cpu.m_mmu->at<T>(addr));
    }
  }

  void execute(Cpu& cpu) {
    Mmu& mmu = *cpu.m_mmu;
    const u32 offset = offset_value(cpu);
    const u32 base_value = cpu.reg(operand_register());
    const u32 addr = select_addr(cpu, base_value, offset);
    printf("addr: %d\n", addr);
    printf("base_value: %d\n", addr);

    const auto transfer_type = static_cast<TransferType>((value & 0x60) >> 5);

    const Register src_or_dest_reg = dest_register();

    if (load()) {
      printf("load\n");
      switch (transfer_type) {
        case TransferType::Swp:
          break;
        case TransferType::UnsignedHalfword:
          run_load<u16>(cpu, addr);
          break;
        case TransferType::SignedByte: {
          run_load<s8>(cpu, addr);
          break;
        }
        case TransferType::SignedHalfword: {
          run_load<s16>(cpu, addr);
          break;
        }
      }
    } else {
      printf("store\n");
      // Store
      switch (transfer_type) {
        case TransferType::Swp:
          break;
        case TransferType::UnsignedHalfword:
          mmu.set(addr, static_cast<u16>(cpu.reg(src_or_dest_reg)));
          break;
        case TransferType::SignedByte:
          mmu.set(addr, static_cast<s8>(cpu.reg(src_or_dest_reg)));
          break;
        case TransferType::SignedHalfword:
          mmu.set(addr, static_cast<s16>(cpu.reg(src_or_dest_reg)));
          break;
      }
    }

    if (!preindex()) {
      run_write_back(cpu, addr);
    }
  }
};

class BlockDataTransfer : public SingleDataTransfer {
 public:
  using SingleDataTransfer::SingleDataTransfer;

  [[nodiscard]] bool load_psr_and_user_mode() const { return test_bit(22); }

  [[nodiscard]] std::array<Register, 4> register_list() const {
    std::array<Register, 4> registers{};
    for (int i = 0; i < 4; ++i) {
      registers[i] = static_cast<Register>(value & (0xf << (i * 4)));
    }
    std::sort(registers.begin(), registers.end(), std::less<>());
    return registers;
  }

  void execute(Cpu& cpu) {
    u32 offset = cpu.reg(operand_register());

    const auto change_offset = add_offset_to_base()
                                   ? [](u32 val) { return val + 4; }
                                   : [](u32 val) { return val - 4; };
    const auto registers = register_list();
    printf("block offset %08x\n", offset);
    if (preindex()) {
      for (const Register reg : registers) {
        offset = change_offset(offset);
        cpu.m_mmu->set(offset, cpu.reg(reg));
      }
    } else {
      for (const Register reg : registers) {
        cpu.m_mmu->set(offset, cpu.reg(reg));
        offset = change_offset(offset);
      }
    }
  }
};

inline void Cpu::execute() {
  const u32 pc = reg(Register::R15) - 4;
  printf("pc: %08x\n", pc);
  const u32 instruction = m_mmu->at<u32>(pc);
  set_reg(Register::R15, pc + 4);

  const InstructionType type = decode_instruction_type(instruction);
  switch (type) {
    case InstructionType::DataProcessing:
      if (pc == 0x08000100) {
        printf("data processing\n");
      }
      run_instruction(DataProcessing{instruction});
      printf("r0: %08x\n", reg(Register::R0));
      break;
    case InstructionType::Mrs:
      run_instruction(Mrs{instruction});
      break;
    case InstructionType::Msr:
      run_instruction(Msr{instruction});
      break;
    case InstructionType::MsrFlagBits:
      run_instruction(MsrFlagBits{instruction});
      break;
    case InstructionType::Multiply:
      run_instruction(Multiply{instruction});
      break;
    case InstructionType::MultiplyLong:
      run_instruction(MultiplyLong{instruction});
      break;
    case InstructionType::Branch:
      run_instruction(Branch{instruction});
      break;
    case InstructionType::BranchAndExchange:
      run_instruction(BranchAndExchange{instruction});
      break;
    case InstructionType::SingleDataTransfer:
      run_instruction(SingleDataTransfer{instruction});
      break;
    case InstructionType::HalfwordDataTransferImm:
    case InstructionType::HalfwordDataTransferReg:
      run_instruction(HalfwordDataTransfer{instruction});
      break;
    case InstructionType::BlockDataTransfer:
      run_instruction(BlockDataTransfer{instruction});
      break;

    default:
      printf("found instruction %08x, type %d\n", instruction, type);
      throw std::runtime_error("unknown instruction type ");
  }
}

namespace tests::cpu {
constexpr void check(bool condition, std::string_view message) {
  if (!condition) {
    throw message;
  }
}
static_assert([]() constexpr->bool {
  // mov r0, #0
  return decode_instruction_type(0xe3a00000) == InstructionType::DataProcessing;
}());

TEST_CASE("mov instructions should move immediate values") {
  Cpu cpu;

  // mov r0, #456
  DataProcessing inst{0xe3a00f72};

  CHECK(inst.dest_register() == Register::R0);

  CHECK(inst.immediate_operand());

  inst.execute(cpu);

  CHECK(cpu.reg(Register::R0) == 456);
}

TEST_CASE("mov instructions should move registers") {
  Cpu cpu;
  cpu.set_reg(Register::R1, 456);

  // mov r0, r1
  DataProcessing inst{0xe1a00001};
  CHECK(inst.dest_register() == Register::R0);

  inst.execute(cpu);

  CHECK(cpu.reg(Register::R0) == 456);
}

static_assert([]() -> bool {
  // mrs r0, cpsr
  const auto type = decode_instruction_type(0xe10f0000);
  return type == InstructionType::Mrs;
}());

TEST_CASE(
    "mrs instructions should transfer the current program status to a "
    "register") {
  Cpu cpu;
  cpu.set_reg(Register::R0, 455);

  cpu.set_carry(true);
  cpu.set_negative(true);

  // mrs r0, cpsr
  Mrs mrs{0xe10f0000};

  CHECK(mrs.dest_register() == Register::R0);

  mrs.execute(cpu);

  CHECK(cpu.program_status().data() == cpu.reg(Register::R0));
}

static_assert(decode_instruction_type(0xe129f000) == InstructionType::Msr);

TEST_CASE(
    "msr instructions should transfer a register to the current program "
    "status") {
  Cpu cpu;
  cpu.set_reg(Register::R0, 456);

  // msr cpsr, r0
  Msr msr{0xe129f000};

  msr.execute(cpu);

  CHECK(cpu.program_status().data() == cpu.reg(Register::R0));
}

// msr cpsr_flg, r0
static_assert(decode_instruction_type(0xe128f000) ==
              InstructionType::MsrFlagBits);

TEST_CASE(
    "msr instructions should only transfer the upper 4 bits of a register "
    "when "
    "specified ") {
  Cpu cpu;

  cpu.set_reg(Register::R0, 0b1010 << 28);

  // msr cpsr_flg, 0
  MsrFlagBits msr{0xe128f000};

  msr.execute(cpu);
  const ProgramStatus program_status = cpu.program_status();

  CHECK(program_status.negative());
  CHECK(!program_status.zero());
  CHECK(program_status.carry());
  CHECK(!program_status.overflow());
}

static_assert(decode_instruction_type(0xe0010392) == InstructionType::Multiply);

TEST_CASE("multiply instructions should multiply") {
  Cpu cpu;

  // mul r1, r2, r3
  Multiply mul{0xe0010392};

  cpu.set_reg(Register::R2, 20);
  cpu.set_reg(Register::R3, 2);

  mul.execute(cpu);

  CHECK(cpu.reg(Register::R1) == 40);
}

TEST_CASE("bl instructions should set r14 equal to r15") {
  Cpu cpu;

  // bl ahead
  // ahead:
  Branch branch{0xebffffff};

  branch.execute(cpu);

  CHECK(cpu.reg(Register::R15) == cpu.reg(Register::R14));
}

constexpr std::array<u8, 4> bytes_test = {0x42, 0x00, 0xa0, 0xe3};
constexpr auto bytes_value = Integer<u32>::from_bytes({bytes_test});
static_assert(bytes_value.data() == 0xe3a00042);

static_assert(
    decode_instruction_type(0b0000'0011'1111'0011'0011'0000'0000'0000) ==
    InstructionType::DataProcessing);
}  // namespace tests::cpu

}  // namespace gb::advance
