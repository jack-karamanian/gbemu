#pragma once
#include <doctest/doctest.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <algorithm>
#include <array>
#include <functional>
#include <nonstd/span.hpp>
#include <optional>
#include <tuple>
#include "error_handling.h"
#include "interrupts.h"
#include "mmu.h"
#include "thumb_to_arm.h"
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

template <typename IntegerType = u32, typename InstType = InstructionType>
struct InstructionLookupEntry {
  IntegerType mask = 0;
  IntegerType expected = 0;
  InstType type = InstType::Undefined;

  constexpr InstructionLookupEntry(InstType type_) : type{type_} {}
  constexpr InstructionLookupEntry() = default;

  template <typename... Args>
  constexpr InstructionLookupEntry& mask_bits(Args... args) {
    mask |= set_bits<IntegerType>(args...);
    return *this;
  }
  constexpr InstructionLookupEntry& mask_bit_range(int begin, int end) {
    for (int i = begin; i <= end; ++i) {
      mask |= (1 << i);
    }
    return *this;
  }

  template <typename... Args>
  constexpr InstructionLookupEntry& expect_bits(Args... args) {
    expected = set_bits<IntegerType>(args...);
    return *this;
  }
};

using LookupEntry = InstructionLookupEntry<u32, InstructionType>;

template <typename IntegerType, typename InstType, auto GenerateTable>
class InstructionTable {
 public:
  constexpr InstructionTable() = default;
  auto decode_instruction_type(IntegerType instruction) const {
    const auto decoded_type = constexpr_find(
        InstructionTable::lookup_table.begin(),
        InstructionTable::lookup_table.end(),
        [instruction](
            const InstructionLookupEntry<IntegerType, InstType>& entry) {
          return (instruction & entry.mask) == entry.expected;
        });

    return decoded_type == lookup_table.end() ? InstType::Undefined
                                              : decoded_type->type;
  }

 private:
  static constexpr auto lookup_table = GenerateTable();
};

enum class Mode {
  User = 0b0000,
  FIQ = 0b0001,
  IRQ = 0b0010,
  Supervisor = 0b0011,
  Abort = 0b0111,
  Undefined = 0b1011,
  System = 0b1111,
};

class ProgramStatus : public Integer<u32> {
 public:
  using Integer::Integer;
  constexpr ProgramStatus()
      : Integer::Integer{static_cast<u32>(Mode::System)} {}

  [[nodiscard]] constexpr bool negative() const { return test_bit(31); }
  constexpr void set_negative(bool set) { set_bit(31, set); }

  [[nodiscard]] constexpr bool zero() const { return test_bit(30); }
  constexpr void set_zero(bool set) { set_bit(30, set); }

  [[nodiscard]] constexpr bool carry() const { return test_bit(29); }
  constexpr void set_carry(bool set) { set_bit(29, set); }

  [[nodiscard]] constexpr bool overflow() const { return test_bit(28); }
  constexpr void set_overflow(bool set) { set_bit(28, set); }

  [[nodiscard]] constexpr Mode mode() const {
    return static_cast<Mode>(m_value & 0b1111);
  }
  constexpr void set_mode(Mode mode) {
    m_value = (m_value & ~0b1111) | static_cast<u32>(mode);
  }

  [[nodiscard]] constexpr bool thumb_mode() const { return test_bit(5); }
  constexpr void set_thumb_mode(bool set) { set_bit(5, set); }

  [[nodiscard]] constexpr bool irq_enabled() const { return !test_bit(7); }
  constexpr void set_irq_enabled(bool set) { set_bit(7, !set); }
};

class Instruction : public Integer<u32> {
 public:
  enum class Condition : u32 {
    EQ = 0b0000,  // Z set
    NE = 0b0001,  // Z clear
    CS = 0b0010,  // C set
    CC = 0b0011,  // C clear
    MI = 0b0100,  // N set
    PL = 0b0101,  // N clear
    VS = 0b0110,  // V set
    VC = 0b0111,  // V clear
    HI = 0b1000,  // C set and Z clear
    LS = 0b1001,  // C clear or Z set
    GE = 0b1010,  // N equals V
    LT = 0b1011,  // N not equal to V
    GT = 0b1100,  // Z clear AND (N equals V)
    LE = 0b1101,  // Z set OR (N not equal to V)
    AL = 0b1110,  // ignored
  };

  using Integer::Integer;

  [[nodiscard]] constexpr Register dest_register() const {
    return static_cast<Register>((m_value >> 12) & 0xf);
  }
  [[nodiscard]] constexpr Register operand_register() const {
    return static_cast<Register>((m_value >> 16) & 0xf);
  }
  [[nodiscard]] constexpr Condition condition() const {
    return static_cast<Condition>((m_value >> 28) & 0xf);
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
    throw std::runtime_error("invalid condition");
  }
};

constexpr std::array<LookupEntry, 18> generate_lookup_table() {
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
          .mask_bits(20, 21)
          .mask_bit_range(23, 27)
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

  constexpr_sort(lookup_table.begin(), lookup_table.end(),
                 [](LookupEntry a, LookupEntry b) { return b.mask < a.mask; });

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
  Cpu() = default;
  Cpu(Mmu& mmu) : m_mmu{&mmu} {}

  constexpr u32 prefetch_offset() const {
    return m_current_program_status.thumb_mode() ? 2 : 4;
  }

  [[nodiscard]] constexpr u32 reg(Register reg_selected) const {
    const u32 index = static_cast<u32>(reg_selected);

    if (reg_selected == Register::R15) {
      const bool thumb_mode = program_status().thumb_mode();
      return (m_regs[index] + prefetch_offset()) & ~(thumb_mode ? 0b1 : 0b11);
    }
    return m_regs[index];
  }

  constexpr void set_reg(Register reg_selected, u32 value) {
    const u32 index = static_cast<u32>(reg_selected);
    if (reg_selected == Register::R15) {
      value &= ~(program_status().thumb_mode() ? 0b1 : 0b11);
    }
    m_regs[index] = value;
  }

  [[nodiscard]] constexpr const ProgramStatus& program_status() const {
    return m_current_program_status;
  }

  constexpr void load_register_range(nonstd::span<const u32> src,
                                     u32 reg_begin,
                                     u32 reg_end) {
    for (u32 i = reg_begin; i <= reg_end; ++i) {
      const auto dest_reg = static_cast<Register>(i);
      set_reg(dest_reg, src[i - reg_begin]);
    }
  }

  constexpr void store_register_range(nonstd::span<u32> dest,
                                      u32 reg_begin,
                                      u32 reg_end) {
    for (u32 i = reg_begin; i <= reg_end; ++i) {
      const auto src_reg = static_cast<Register>(i);
      dest[i - reg_begin] = reg(src_reg);
    }
  }

  void change_mode(Mode next_mode) {
    const Mode current_mode = m_current_program_status.mode();
    const auto select_mode_storage = [this](Mode mode_) -> nonstd::span<u32> {
      switch (mode_) {
        case Mode::FIQ:
          return m_saved_registers.fiq;
        case Mode::Supervisor:
          return m_saved_registers.supervisor;
        case Mode::Abort:
          return m_saved_registers.abort;
        case Mode::IRQ:
          return m_saved_registers.irq;
        case Mode::Undefined:
          return m_saved_registers.undefined;
        case Mode::System:
        case Mode::User:
          return m_saved_registers.system_and_user;
        default:
          throw std::runtime_error("invalid mode");
      }
    };

    // Find range of registers to store
    const auto select_mode_range = [](Mode mode) -> std::pair<u32, u32> {
      switch (mode) {
        case Mode::FIQ:
          return {8, 14};
        case Mode::Supervisor:
        case Mode::Abort:
        case Mode::IRQ:
        case Mode::Undefined:
          return {13, 14};
        default:
          // User & system have no banked registers.
          // Determine which registers are stored from the other modes.
          return {1, 0};
      }
    };
    const auto [reg_begin, reg_end] =
        (next_mode == Mode::User || next_mode == Mode::System) &&
                (current_mode != Mode::User && current_mode != Mode::System)
            ? select_mode_range(current_mode)
            : select_mode_range(next_mode);

    auto current_saved_register_storage = select_mode_storage(current_mode);
    auto next_saved_register_storage = select_mode_storage(next_mode);

    store_register_range(current_saved_register_storage, reg_begin, reg_end);
    load_register_range(next_saved_register_storage, reg_begin, reg_end);

    get_current_program_status().set_mode(next_mode);
  }

  constexpr void set_program_status(ProgramStatus status) {
    const Mode current_mode = m_current_program_status.mode();
    const Mode next_mode = status.mode();
    if (current_mode != next_mode) {
      change_mode(next_mode);
    }
    m_current_program_status = status;
  }

  [[nodiscard]] constexpr ProgramStatus saved_program_status() const {
    assert_in_user_mode();
    return m_saved_program_status[index_from_mode(
        m_current_program_status.mode())];
  }

  constexpr void set_saved_program_status(ProgramStatus program_status) {
    assert_in_user_mode();
    m_saved_program_status[index_from_mode(m_current_program_status.mode())] =
        program_status;
  }

  constexpr void set_saved_program_status_for_mode(
      Mode mode,
      ProgramStatus program_status) {
    m_saved_program_status[index_from_mode(mode)] = program_status;
  }

  constexpr void move_spsr_to_cpsr() {
    const Mode mode = m_current_program_status.mode();
    if (mode != Mode::User && mode != Mode::System) {
      ProgramStatus saved_program_status =
          m_saved_program_status[static_cast<u32>(mode) - 1];
      set_program_status(saved_program_status);
    } else {
      abort();
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
  constexpr void set_thumb(bool set) {
    m_prefetch_offset = set ? 2 : 4;
    get_current_program_status().set_thumb_mode(set);
  }

  InterruptBucket interrupts_enabled{0};
  InterruptBucket interrupts_requested{0};
  u32 ime = 0;

  template <typename T>
  constexpr u32 run_instruction(T instruction) {
    if (instruction.should_execute(program_status())) {
      return instruction.execute(*this);
    }
    return 0;
  }

  inline u32 execute();
  u32 handle_interrupts();

  friend class DataProcessing;
  friend class SingleDataTransfer;
  friend class HalfwordDataTransfer;
  friend class BlockDataTransfer;
  friend class SingleDataSwap;
  friend class SoftwareInterrupt;

 private:
  struct SavedRegisters {
    // R8-R14
    std::array<u32, 7> system_and_user{{0, 0, 0, 0, 0, 0, 0}};
    std::array<u32, 7> fiq{{0, 0, 0, 0, 0, 0, 0}};
    // R13-R14
    std::array<u32, 2> supervisor{{0, 0}};
    std::array<u32, 2> abort{{0, 0}};
    std::array<u32, 2> irq{{0, 0}};
    std::array<u32, 2> undefined{{0, 0}};
  };

  constexpr ProgramStatus& get_current_program_status() {
    return m_current_program_status;
  }
  constexpr void assert_in_user_mode() const {
    const Mode mode = m_current_program_status.mode();
    if (mode == Mode::User || mode == Mode::System) {
      throw std::runtime_error(
          "saved program status can not be accessed in user mode");
    }
  }

  [[nodiscard]] constexpr u32 index_from_mode(Mode mode) const {
    return static_cast<u32>(mode) - 1;
    // return static_cast<u32>(m_current_program_status.mode()) - 1;
  }
  std::array<u32, 16> m_regs = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  Mmu* m_mmu = nullptr;
  Mode m_mode = Mode::User;
  ProgramStatus m_current_program_status{};
  std::array<ProgramStatus, 5> m_saved_program_status{};
  SavedRegisters m_saved_registers;
  u32 m_prefetch_offset = 4;
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
  const u32 reg_value =
      cpu.reg(reg) +
      (register_specified && reg == Register::R15 ? 4 : 0);  // Rm

  // TODO: Finish shift carry calculation
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
  switch (shift_type) {
    case ShiftType::LogicalLeft:
      return shift_amount != 0 &&
             gb::test_bit(shift_operand, 32 - shift_amount);
    case ShiftType::LogicalRight:
      return gb::test_bit(shift_operand, shift_amount - 1);
    case ShiftType::ArithmeticRight:
      return gb::test_bit(shift_operand, shift_amount - 1);
    case ShiftType::RotateRight:
      return gb::test_bit(shift_operand, shift_amount - 1);
  }
  GB_UNREACHABLE();
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
    const ShiftResult& shift_result) {
  const auto [shift_type, shift_amount, shift_operand, result, set_carry] =
      shift_result;

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
    return static_cast<Opcode>((m_value >> 21) & 0xf);
  }

  [[nodiscard]] constexpr bool set_condition_code() const {
    return test_bit(20);
  }

  [[nodiscard]] constexpr u8 immediate_value() const {
    return static_cast<u8>(m_value & 0xff);
  }

  [[nodiscard]] constexpr u8 immediate_shift() const {
    return static_cast<u8>((m_value >> 8) & 0xf);
  }

  [[nodiscard]] constexpr ShiftResult shift_value(const Cpu& cpu) const {
    if (immediate_operand()) {
      return {ShiftType::RotateRight,
              static_cast<u8>(((m_value >> 8) & 0xf) * 2),
              m_value & 0xff,
              {},
              {}};
    }
    return compute_shift_value(m_value, cpu);
  }

  [[nodiscard]] Cycles cycles() const {
    const bool register_shift = !immediate_operand() && test_bit(4);
    if (register_shift && dest_register() == Register::R15) {
      return 2_seq + 1_nonseq + 1_intern;
    } else if (register_shift) {
      return 1_seq + 1_intern;
    } else if (dest_register() == Register::R15) {
      return 2_seq + 1_nonseq;
    }
    return 1_seq;
  }

  [[nodiscard]] constexpr std::tuple<bool, u32> compute_operand2(
      const Cpu& cpu) const {
    const ShiftResult shift_result = shift_value(cpu);
    return compute_shifted_operand(shift_result);
  }

  u32 execute(Cpu& cpu) {
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

    const auto run_arithmetic = [&](u64 op1, u64 op2, bool write, auto impl,
                                    bool invert_carry = false) {
      const u32 dest_value = cpu.reg(dest_register());

      const u64 result = impl(op1, op2);

      const u32 result_32 = static_cast<u32>(result & 0xffffffff);
      if (set_condition_code()) {
        cpu.set_overflow((gb::test_bit(op1, 31) == gb::test_bit(op2, 31)) &&
                         gb::test_bit(op1, 31) != gb::test_bit(result, 31)

        );
#if 0
        cpu.set_overflow(
            gb::test_bit(static_cast<u32>(result & 0xffffffff), 31) &&
            gb::test_bit(dest_value, 31) !=
                gb::test_bit(static_cast<u32>(result & 0xffffffff), 31));
#endif
        cpu.set_zero(result_32 == 0);
        cpu.set_negative(gb::test_bit(result_32, 31));
        cpu.set_carry(invert_carry ? result <= std::numeric_limits<u32>::max()
                                   : result > std::numeric_limits<u32>::max());
      }

      if (write) {
        write_result(dest_reg, result_32);
      }
    };

    switch (opcode()) {
      // Arithmetic
      case Opcode::Sub:
        run_arithmetic(operand1, -operand2, true,
                       [](u64 op1, u64 op2) { return op1 + op2; });
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
        run_arithmetic(
            operand1, operand2, true,
            [carry_value](u64 op1, u64 op2) {
              return op1 - op2 + carry_value - 1;
            },
            true);
        break;
        // return op1 - op2 + carry_value - 1;
      case Opcode::Rsc:
        run_arithmetic(
            operand1, operand2, true,
            [carry_value](u64 op1, u64 op2) {
              // fmt::print("op1: {}, op2: {}\n", op1, op2);
              // fmt::print("res {}\n", op2 - op1 + carry_value - 1);
              return op2 - op1 + carry_value - 1;
            },
            true);
        break;
        // return op2 - op1 + carry_value - 1;
      case Opcode::Cmp:
        run_arithmetic(
            operand1, -operand2, false,
            [](u64 op1, u64 op2) { return op1 + op2; }, true);
        break;
      case Opcode::Cmn:
        run_arithmetic(operand1, operand2, false,
                       [](u64 op1, u64 op2) { return op1 + op2; });
        break;
      // Logical
      case Opcode::And:
        // fmt::printf("op1: %d, op2: %d\n", operand1, operand2);
        // fmt::printf("res: %08x\n", operand1 & operand2);
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

    return cycles().sum();
  }
};

class Mrs : public Instruction {
 public:
  using Instruction::Instruction;

  u32 execute(Cpu& cpu) {
    const u32 program_status =
        (test_bit(22) ? cpu.saved_program_status() : cpu.program_status())
            .data();

    cpu.set_reg(dest_register(), program_status);

    // 1S
    return 1;
  }
};

class Msr : public Instruction {
 public:
  using Instruction::Instruction;

  u32 execute(Cpu& cpu) {
    const ProgramStatus program_status{
        cpu.reg(static_cast<Register>(m_value & 0xf))};
    if (test_bit(22)) {
      cpu.set_saved_program_status(program_status);
    } else {
      cpu.set_program_status(program_status);
    }

    // 1S
    return 1;
  }
};

class MsrFlagBits : public DataProcessing {
 public:
  using DataProcessing::DataProcessing;

  u32 execute(Cpu& cpu) {
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

    // 1S
    return 1;
  }
};

class Multiply : public Instruction {
 public:
  using Instruction::Instruction;
  [[nodiscard]] constexpr Register dest_register() const {
    return static_cast<Register>((m_value >> 16) & 0xf);
  }

  [[nodiscard]] constexpr Register lhs_register() const {
    return static_cast<Register>(m_value & 0xf);
  }

  [[nodiscard]] constexpr Register rhs_register() const {
    return static_cast<Register>((m_value >> 8) & 0xf);
  }

  [[nodiscard]] constexpr Register accumulate_register() const {
    return static_cast<Register>((m_value >> 12) & 0xf);
  }

  [[nodiscard]] constexpr bool set_condition_code() const {
    return test_bit(20);
  }

  [[nodiscard]] constexpr bool accumulate() const { return test_bit(21); }

  [[nodiscard]] Cycles cycles(u32 rhs_operand) const {
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

    return Cycles{1, 0, multiply_cycles + (accumulate() ? 1 : 0)};
  }

  u32 execute(Cpu& cpu) {
    const u32 rhs_operand = cpu.reg(rhs_register());
    const u32 res =
        static_cast<u32>(static_cast<u64>(cpu.reg(lhs_register())) *
                             static_cast<u64>(rhs_operand) +
                         (accumulate() ? cpu.reg(accumulate_register()) : 0));

    cpu.set_reg(dest_register(), res);

    if (set_condition_code()) {
      cpu.set_zero(res == 0);
      cpu.set_negative(gb::test_bit(res, 31));
    }

    return cycles(rhs_operand).sum();
  }
};

class MultiplyLong : public Multiply {
 public:
  using Multiply::Multiply;

  [[nodiscard]] constexpr Register dest_register_high() const {
    return static_cast<Register>((m_value >> 16) & 0xf);
  }

  [[nodiscard]] constexpr Register dest_register_low() const {
    return static_cast<Register>((m_value >> 12) & 0xf);
  }

  [[nodiscard]] constexpr bool is_signed() const { return test_bit(22); }

  [[nodiscard]] constexpr u64 accumulate_value(const Cpu& cpu) const {
    if (!accumulate()) {
      return 0;
    }

    return (static_cast<u64>(cpu.reg(dest_register_high())) << 32) |
           static_cast<u64>(cpu.reg(dest_register_low()));
  }

  [[nodiscard]] Cycles cycles(u32 rhs_operand) const {
    if (is_signed()) {
      return this->Multiply::cycles(rhs_operand);
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

    return {1, 0, multiply_cycles + (accumulate() ? 2 : 1)};
  }

  template <typename T>
  [[nodiscard]] constexpr T multiply(T lhs, T rhs) {
    static_assert(std::is_integral_v<T>);

    return lhs * rhs;
  }

  u32 execute(Cpu& cpu) {
    const u32 lhs = cpu.reg(lhs_register());
    const u32 rhs = cpu.reg(rhs_register());

    const u64 res = (is_signed() ? multiply<s64>(static_cast<s32>(lhs),
                                                 static_cast<s32>(rhs))
                                 : multiply<u64>(lhs, rhs)) +
                    accumulate_value(cpu);

    cpu.set_reg(dest_register_high(), (res & 0xffffffff00000000) >> 32);
    cpu.set_reg(dest_register_low(), res & 0xffffffff);

    if (set_condition_code()) {
      cpu.set_zero(res == 0);
      cpu.set_negative(gb::test_bit(res, 63));
    }
    return cycles(rhs).sum();
  }
};

class Branch : public Instruction {
 public:
  using Instruction::Instruction;
  u32 execute(Cpu& cpu) {
    const bool link = test_bit(24);
    const bool negative = test_bit(23);

    const bool thumb_mode = cpu.program_status().thumb_mode();
    // Convert 24 bit signed to 32 bit signed
    const s32 offset =
        ((m_value & 0b0111'1111'1111'1111'1111'1111)
         << (cpu.program_status().thumb_mode() ? 0 : 2)) |
        (negative ? (thumb_mode ? 0xff800000 : (0xfe << 24)) : 0);

    const u32 next_pc = cpu.reg(Register::R15) + offset;

    if (link) {
      cpu.set_reg(Register::R14, cpu.reg(Register::R15) - 4);
    }
    cpu.set_reg(Register::R15, next_pc);
    return (2_seq + 1_nonseq).sum();
  }
};

class BranchAndExchange : public Instruction {
 public:
  using Instruction::Instruction;

  u32 execute(Cpu& cpu) {
    const auto next_pc_reg = static_cast<Register>(m_value & 0xf);
    const u32 reg_value = cpu.reg(next_pc_reg);
    cpu.set_reg(Register::R15, reg_value);
    const bool thumb_mode = gb::test_bit(reg_value, 0);
    cpu.set_thumb(thumb_mode);

    return (2_seq + 1_nonseq).sum();
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

  [[nodiscard]] Cycles cycles() const {
    // Store
    if (!load()) {
      return 2_nonseq;
    }

    // Load
    return dest_register() == Register::R15 ? (2_seq + 2_nonseq + 1_intern)
                                            : (1_seq + 1_nonseq + 1_intern);
  }

  constexpr void run_write_back(Cpu& cpu, u32 addr) {
    const Register base_register = operand_register();
    cpu.set_reg(base_register, addr);
  }

  [[nodiscard]] constexpr u32 calculate_addr(u32 base_value, u32 offset) const {
    const u32 addr =
        add_offset_to_base() ? offset + base_value : base_value - offset;
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

  u32 execute(Cpu& cpu) {
    Mmu& mmu = *cpu.m_mmu;
    const auto calculate_offset = [&]() -> u32 {
      if (immediate_offset()) {
        return m_value & 0xfff;
      }

      const ShiftResult shift_result = compute_shift_value(m_value, cpu);
      const auto [set_carry, offset] = compute_shifted_operand(shift_result);

      return offset;
    };

    const Register base_register = operand_register();
    const u32 base_value =
        cpu.reg(base_register) &
        // Set bit 1 of R15 to 0 when in thumb mode
        ~(cpu.program_status().thumb_mode() && base_register == Register::R15
              ? 0b10
              : 0);

    const u32 offset = calculate_offset();

    // The address to be used in the transfer
    const auto [aligned_addr, raw_addr,
                rotate_amount] = [&]() -> std::tuple<u32, u32, u32> {
      const u32 raw_addr = select_addr(cpu, base_value, offset);
      return {raw_addr & ~0b11, raw_addr, (raw_addr & 0b11) * 8};
    }();

    if (!preindex()) {
      const u32 writeback_addr = calculate_addr(base_value, offset);
      run_write_back(cpu, writeback_addr);
    }

    if (load()) {
      if (word()) {
        // Load a word
        const u32 loaded_value =
            rotate_right(mmu.at<u32>(aligned_addr & ~0b11), rotate_amount);
        cpu.set_reg(dest_register(), loaded_value);
      } else {
        // Load a byte
        const u8 loaded_value = mmu.at<u8>(raw_addr);
        cpu.set_reg(dest_register(), loaded_value);
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

    return mmu.wait_cycles(aligned_addr, cycles());
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
      return ((m_value & 0xf00) >> 4) | (m_value & 0xf);
    }

    const auto offset_reg = static_cast<Register>(m_value & 0xf);
    return cpu.reg(offset_reg);
  }

  template <typename T>
  constexpr void run_load(Cpu& cpu, u32 addr) {
    const Register dest_reg = dest_register();

    if constexpr (std::is_signed_v<T>) {
      cpu.set_reg(dest_reg, static_cast<s32>(cpu.m_mmu->at<T>(addr)));
    } else {
      cpu.set_reg(dest_reg, cpu.m_mmu->at<T>(addr));
    }
  }

  u32 execute(Cpu& cpu) {
    Mmu& mmu = *cpu.m_mmu;
    const u32 offset = offset_value(cpu);
    const u32 base_value = cpu.reg(operand_register());
    const u32 addr = select_addr(cpu, base_value, offset);

    const auto transfer_type = static_cast<TransferType>((m_value & 0x60) >> 5);

    const Register src_or_dest_reg = dest_register();

    if (load()) {
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
      const u32 writeback_addr = calculate_addr(base_value, offset);
      run_write_back(cpu, writeback_addr);
    }

    return mmu.wait_cycles(addr, cycles());
  }
};

class BlockDataTransfer : public SingleDataTransfer {
 public:
  using SingleDataTransfer::SingleDataTransfer;

  [[nodiscard]] bool load_psr_and_user_mode() const { return test_bit(22); }

  [[nodiscard]] std::tuple<std::array<Register, 16>, int> register_list()
      const {
    std::array<Register, 16> registers{};
    int end = 0;

    const auto append_register = [&](int i) {
      if (test_bit(i)) {
        registers[end++] = static_cast<Register>(i);
      }
    };

    if (!add_offset_to_base()) {
      for (int i = 15; i >= 0; --i) {
        append_register(i);
      }
    } else {
      for (int i = 0; i < 16; ++i) {
        append_register(i);
      }
    }
    return {registers, end};
  }

  u32 execute(Cpu& cpu) {
    const Mode current_mode = cpu.get_current_program_status().mode();
    if (load_psr_and_user_mode()) {
      cpu.change_mode(Mode::User);
    }
    u32 offset = cpu.reg(operand_register());

    const auto change_offset = add_offset_to_base()
                                   ? [](u32 val) { return val + 4; }
                                   : [](u32 val) { return val - 4; };
    const auto [registers, registers_end] = register_list();
    const nonstd::span<const Register> registers_span{registers.data(),
                                                      registers_end};

    u32 addr_cycles = 0;
    const u32 absolute_offset = 4 * registers_end;
    const u32 final_offset = add_offset_to_base() ? (offset + absolute_offset)
                                                  : (offset - absolute_offset);

    const bool regs_has_base =
        std::find(registers_span.begin(), registers_span.end(),
                  operand_register()) != registers_span.end();

    if (write_back()) {
      if (load()) {
        cpu.set_reg(operand_register(), final_offset);
      } else if (regs_has_base &&
                 ((!add_offset_to_base() &&
                   registers[registers_end - 1] != operand_register()) ||
                  (add_offset_to_base() &&
                   registers[0] != operand_register()))) {
        fmt::print("REG[0] {}\n", static_cast<u32>(registers[0]));
        fmt::print("REG[-1] {}\n",
                   static_cast<u32>(registers[registers_end - 1]));
        fmt::print("base reg {}\n", static_cast<u32>(operand_register()));
        cpu.set_reg(operand_register(), final_offset);

        // if (add)
      }
    }

    for (const Register reg : registers_span) {
#if 0
      if (write_back() && !load() && reg == operand_register() && i > 0) {
        cpu.set_reg(operand_register(), final_offset);
      }
#endif
      if (preindex()) {
        offset = change_offset(offset);
      }

      addr_cycles += cpu.m_mmu->wait_cycles(offset, 1_seq);
      if (load()) {
        cpu.set_reg(reg, cpu.m_mmu->at<u32>(offset));
      } else {
        cpu.m_mmu->set(offset, cpu.reg(reg));
      }

      if (!preindex()) {
        offset = change_offset(offset);
      }
    }

    if (!load() && write_back()) {
      cpu.set_reg(operand_register(), final_offset);
    }

    if (load_psr_and_user_mode()) {
      cpu.change_mode(current_mode);
    }

    return addr_cycles + (1_nonseq + 1_intern).sum();
  }
};

class SingleDataSwap : public Instruction {
 public:
  using Instruction::Instruction;

  [[nodiscard]] bool swap_byte() const { return test_bit(22); }

  [[nodiscard]] Register source_register() const {
    return static_cast<Register>(m_value & 0xf);
  }

  template <typename T>
  u32 swap(Cpu& cpu) {
    constexpr Cycles cycles = 1_seq + 2_nonseq + 1_intern;

    const u32 base_value = cpu.reg(operand_register());

    T mem_value = cpu.m_mmu->at<T>(base_value);
    u32 reg_value = cpu.reg(source_register());

    if constexpr (std::is_same_v<T, u8>) {
      const u32 shift_amount = (base_value % 4) * 8;
      const u32 mask = 0xff << shift_amount;
      const u8 byte_value =
          static_cast<u8>(((reg_value & mask) >> shift_amount) & 0xff);
      cpu.m_mmu->set(base_value, byte_value);
    } else {
      cpu.m_mmu->set(base_value, reg_value);
    }
    cpu.set_reg(dest_register(), mem_value);

    return cpu.m_mmu->wait_cycles(base_value, cycles);
  }

  u32 execute(Cpu& cpu) {
    if (swap_byte()) {
      return swap<u8>(cpu);
    }
    return swap<u32>(cpu);
  }
};

class SoftwareInterrupt : public Instruction {
 public:
  using Instruction::Instruction;

  u32 execute(Cpu& cpu);

 private:
  void cpu_set(Cpu& cpu);
};

inline u32 Cpu::execute() {
  const u32 instruction = [this] {
    if (m_current_program_status.thumb_mode()) {
      static constexpr u32 nop = 0b1110'00'0'1101'0'0000'0000'000000000000;
      const u32 pc = reg(Register::R15) - 2;
      const u16 instruction = m_mmu->at<u16>(pc);
      // fmt::printf("%08x\n", pc);

      // Long branch with link
      if ((instruction & 0xf000) == 0xf000) {
        const u32 offset = (instruction & 0x7ff);
        if (!test_bit(instruction, 11)) {
          const s32 signed_offset =
              (offset << 12) | (test_bit(offset, 10) ? 0xff800000 : 0);
          set_reg(Register::R14, pc + signed_offset);
          set_reg(Register::R15, pc + 2);
          return nop;
        }

        const u32 next_instruction = pc + 2;
        set_reg(Register::R15, reg(Register::R14) + (offset << 1) + 4);
        set_reg(Register::R14, next_instruction | 1);
        return nop;
      }
      set_reg(Register::R15, pc + 2);
      // fmt::printf("next pc %08x\n", reg(Register::R15) - 2);
      return convert_thumb_to_arm(instruction);
    }

    const u32 pc = reg(Register::R15) - 4;
    const u32 instruction = m_mmu->at<u32>(pc);
    set_reg(Register::R15, pc + 4);
    return instruction;
  }();

  const InstructionType type = decode_instruction_type(instruction);
  switch (type) {
    case InstructionType::DataProcessing:
      return run_instruction(DataProcessing{instruction});
    case InstructionType::Mrs:
      return run_instruction(Mrs{instruction});
    case InstructionType::Msr:
      return run_instruction(Msr{instruction});
    case InstructionType::MsrFlagBits:
      return run_instruction(MsrFlagBits{instruction});
    case InstructionType::Multiply:
      return run_instruction(Multiply{instruction});
    case InstructionType::MultiplyLong:
      return run_instruction(MultiplyLong{instruction});
    case InstructionType::Branch:
      return run_instruction(Branch{instruction});
    case InstructionType::BranchAndExchange:
      return run_instruction(BranchAndExchange{instruction});
    case InstructionType::SingleDataTransfer:
      return run_instruction(SingleDataTransfer{instruction});
    case InstructionType::HalfwordDataTransferImm:
    case InstructionType::HalfwordDataTransferReg:
      return run_instruction(HalfwordDataTransfer{instruction});
    case InstructionType::BlockDataTransfer:
      return run_instruction(BlockDataTransfer{instruction});
    case InstructionType::SingleDataSwap:
      return run_instruction(SingleDataSwap{instruction});
    case InstructionType::SoftwareInterrupt:
      return run_instruction(SoftwareInterrupt{instruction});

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
  cpu.set_reg(Register::R0, static_cast<u32>(Mode::Supervisor));

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

  // CHECK(cpu.reg(Register::R15) == cpu.reg(Register::R14));
}

constexpr std::array<u8, 4> bytes_test = {0x42, 0x00, 0xa0, 0xe3};
constexpr auto bytes_value = Integer<u32>::from_bytes({bytes_test});
static_assert(bytes_value.data() == 0xe3a00042);

static_assert(
    decode_instruction_type(0b0000'0011'1111'0011'0011'0000'0000'0000) ==
    InstructionType::DataProcessing);
}  // namespace tests::cpu

}  // namespace gb::advance
