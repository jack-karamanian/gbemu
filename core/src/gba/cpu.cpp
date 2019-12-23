#include "gba/cpu.h"
#include "gba/hle.h"

namespace gb::advance {
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
    const ShiftResult shift_result) {
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
              cpu.program_status().carry()};
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
    //
    const Register dest_reg = dest_register();

    const u32 operand1 =
        cpu.reg(operand_register()) &
        // Mask off bit 1 of R15 for thumb "adr #xx" instructions
        ~(cpu.program_status().thumb_mode() &&
                  operand_register() == Register::R15 &&
                  dest_reg != Register::R15
              ? 0b10
              : 0);

    const u32 carry_value = cpu.carry();

    const auto write_result = [&cpu, this](Register dest, u32 result) {
      if (dest == Register::R15 && set_condition_code()) {
        cpu.move_spsr_to_cpsr();
      }
      cpu.set_reg(dest, result);
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

    const auto run_arithmetic =
        [&](u64 op1, u64 op2, bool write, FunctionRef<u64(u64, u64)> impl,
            bool invert_carry, bool carry_override = false,
            bool carry_override_value = false) {
          const u64 result = impl(op1, op2);

          const u32 result_32 = static_cast<u32>(result & 0xffffffff);
          if (set_condition_code()) {
            const bool overflow = invert_carry
                                      ? (((op1 ^ op2) >> 31) & 0b1) &&
                                            (((op1 ^ result_32) >> 31) & 0b1)
                                      : (!(((op1 ^ op2) >> 31) & 0b1)) &&
                                            (((op2 ^ result_32) >> 31) & 0b1);
            cpu.set_overflow(overflow);
            cpu.set_zero(result_32 == 0);
            cpu.set_negative(gb::test_bit(result_32, 31));
            if (carry_override) {
              cpu.set_carry(carry_override_value);
            } else {
              cpu.set_carry(
                  (invert_carry ? (op2 <= op1) : gb::test_bit(result, 32)));
            }
          }

          if (write) {
            write_result(dest_reg, result_32);
          }
        };

    switch (opcode()) {
      // Arithmetic
      case Opcode::Sub:
        run_arithmetic(
            operand1, operand2, true,
            [](u64 op1, u64 op2) { return op1 - op2; }, true);
        break;
      case Opcode::Rsb:
        // op2 - op1
        run_arithmetic(
            operand2, operand1, true,
            [](u64 op1, u64 op2) { return op1 - op2; }, true);
        break;
      case Opcode::Add:
        run_arithmetic(
            operand1, operand2, true,
            [](u64 op1, u64 op2) { return op1 + op2; }, false);
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
            static_cast<u64>(operand2) + static_cast<u64>(carry_value) - 1 <=
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
            operand1, operand2, false,
            [](u64 op1, u64 op2) { return op1 - op2; }, true);
        break;
      case Opcode::Cmn:
        run_arithmetic(
            operand1, operand2, false,
            [](u64 op1, u64 op2) { return op1 + op2; }, false);
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
        ((m_value & 0b0111'1111'1111'1111'1111'1111) << (thumb_mode ? 0 : 2)) |
        (negative ? (thumb_mode ? 0xff800000 : (0xfe << 24)) : 0);

    const u32 next_pc = cpu.reg(Register::R15) + offset;

    if (link) {
      cpu.set_reg(Register::R14, cpu.reg(Register::R15) - 4);
    }
    cpu.set_reg(Register::R15, next_pc);
    return 3;
  }
};

class BranchAndExchange : public Instruction {
 public:
  using Instruction::Instruction;

  u32 execute(Cpu& cpu) {
    const auto next_pc_reg = static_cast<Register>(m_value & 0xf);
    const u32 reg_value = cpu.reg(next_pc_reg);

    const bool thumb_mode = gb::test_bit(reg_value, 0);
    cpu.set_thumb(thumb_mode);

    cpu.set_reg(Register::R15, reg_value);

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
      const u32 raw_address = select_addr(cpu, base_value, offset);
      return {raw_address & ~0b11, raw_address, (raw_address & 0b11) * 8};
    }();

    if (!preindex()) {
      const u32 writeback_addr = calculate_addr(base_value, offset);
      run_write_back(cpu, writeback_addr);
    }

    const bool word_transfer = word();
    const auto dest = dest_register();

    if (load()) {
      if (word_transfer) {
        // Load a word
        const u32 loaded_value =
            rotate_right(mmu.at<u32>(aligned_addr & ~0b11), rotate_amount);
        cpu.set_reg(dest, loaded_value);
      } else {
        // Load a byte
        const u8 loaded_value = mmu.at<u8>(raw_addr);
        cpu.set_reg(dest, loaded_value);
      }
    } else {
      const u32 stored_value = cpu.reg(dest_register());
      if (word_transfer) {
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
    const auto operand_reg = operand_register();
    u32 offset = cpu.reg(operand_reg);

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
        std::find(registers_span.begin(), registers_span.end(), operand_reg) !=
        registers_span.end();

    const bool load_register = load();
    const bool write_back_base = write_back();
    const bool preindex_base = preindex();
    const bool add_offset = add_offset_to_base();

    if (write_back_base) {
      if (load_register) {
        cpu.set_reg(operand_register(), final_offset);
      } else if (regs_has_base &&
                 ((!add_offset &&
                   registers[registers_end - 1] != operand_reg) ||
                  (add_offset && registers[0] != operand_reg))) {
        cpu.set_reg(operand_reg, final_offset);
      }
    }

    for (const Register reg : registers_span) {
      if (preindex_base) {
        offset = change_offset(offset);
      }

      addr_cycles += cpu.m_mmu->wait_cycles(offset, 1_seq);
      if (load_register) {
        cpu.set_reg(reg, cpu.m_mmu->at<u32>(offset));
      } else {
        cpu.m_mmu->set(offset, cpu.reg(reg));
      }

      if (!preindex_base) {
        offset = change_offset(offset);
      }
    }

    if (!load_register && write_back_base) {
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
static void intr_wait(Cpu& cpu, u32 set_flags, u32 interrupt_flags) {
  cpu.ime = 1;

  if (set_flags != 0 || cpu.interrupts_waiting.data() == 0) {
    cpu.interrupts_waiting.set_data(static_cast<u16>(interrupt_flags));
  }
}

enum class SoftwareInterruptType : u32 {
  SoftReset = 0x0,
  RegisterRamReset = 0x1,
  Halt = 0x02,
  VBlankIntrWait = 0x05,
  Div = 0x06,
  Sqrt = 0x08,
  ArcTan2 = 0x0a,
  CpuSet = 0x0b,
  CpuFastSet = 0x0c,
  BgAffineSet = 0xe,
  ObjAffineSet = 0x0f,
  Lz77Wram = 0x11,
  Lz77Vram = 0x12,
  MidiKeyToFreq = 0x1f,
  SoundDriverVSyncOff = 0x28,
  NonstdStopExecution = 0xff,
};

u32 SoftwareInterrupt::execute(Cpu& cpu) {
  using namespace gb::advance::hle::bios;

  const auto interrupt_type = static_cast<SoftwareInterruptType>(
      (m_value & 0x00ffffff) >> (cpu.program_status().thumb_mode() ? 0 : 16));

  switch (interrupt_type) {
    case SoftwareInterruptType::SoftReset:
      cpu.soft_reset();
      break;
    case SoftwareInterruptType::RegisterRamReset:
      break;
    case SoftwareInterruptType::Halt:
      cpu.halted = true;
      break;
    case SoftwareInterruptType::VBlankIntrWait:
      intr_wait(cpu, 1, 1);
      break;
    case SoftwareInterruptType::Div: {
      const auto [div, mod, abs_div] =
          divide(cpu.reg(Register::R0), cpu.reg(Register::R1));
      cpu.set_reg(Register::R0, div);
      cpu.set_reg(Register::R1, mod);
      cpu.set_reg(Register::R3, abs_div);
      break;
    }
    case SoftwareInterruptType::Sqrt: {
      const u16 res = std::sqrt(static_cast<float>(cpu.reg(Register::R0)));
      cpu.set_reg(Register::R0, res);
      break;
    }
    case SoftwareInterruptType::ArcTan2: {
      const s16 x = static_cast<s16>(cpu.reg(Register::R0) & 0xffff);
      const s16 y = static_cast<s16>(cpu.reg(Register::R1) & 0xffff);
      cpu.set_reg(Register::R0, arctan2(x, y));
      break;
    }
    case SoftwareInterruptType::CpuSet:
      cpu_set(*cpu.m_mmu, cpu.reg(Register::R0), cpu.reg(Register::R1),
              cpu.reg(Register::R2));
      break;
    case SoftwareInterruptType::CpuFastSet:
      cpu_fast_set(*cpu.m_mmu, cpu.reg(Register::R0), cpu.reg(Register::R1),
                   cpu.reg(Register::R2));
      break;
    case SoftwareInterruptType::BgAffineSet:
      break;
    case SoftwareInterruptType::ObjAffineSet:
      break;
    case SoftwareInterruptType::Lz77Wram: {
      // fmt::printf("source %08x dest %08x\n", cpu.reg(Register::R0),
      //            cpu.reg(Register::R1));
      const auto [source_storage, source_addr] =
          cpu.m_mmu->select_storage(cpu.reg(Register::R0));
      const auto [dest_storage, dest_addr] =
          cpu.m_mmu->select_storage(cpu.reg(Register::R1));
#if 0
      lz77_decompress(source_storage.subspan(source_addr), dest_storage,
                      dest_addr, 1);
#else
      lz77_decompress(source_storage.subspan(source_addr),
                      dest_storage.subspan(dest_addr), dest_addr, 1);
#endif
      break;
    }
    case SoftwareInterruptType::Lz77Vram: {
      // fmt::printf("source %08x dest %08x\n", cpu.reg(Register::R0),
      //            cpu.reg(Register::R1));
      const auto [source_storage, source_addr] =
          cpu.m_mmu->select_storage(cpu.reg(Register::R0));
      const auto [dest_storage, dest_addr] =
          cpu.m_mmu->select_storage(cpu.reg(Register::R1) & ~1);
#if 0
      lz77_decompress(source_storage.subspan(source_addr), dest_storage,
                      dest_addr, 2);
#else
      lz77_decompress(source_storage.subspan(source_addr),
                      dest_storage.subspan(dest_addr), dest_addr, 2);
#endif
      break;
    }
    case SoftwareInterruptType::MidiKeyToFreq: {
      // From mgba
      auto freq = cpu.m_mmu->at<u32>(cpu.reg(Register::R0) + 4) /

                  std::exp2f((180.0f - cpu.reg(Register::R1) -
                              cpu.reg(Register::R2) / 256.0f) /
                             12.0f);
      cpu.set_reg(Register::R0, freq);
      break;
    }
    case SoftwareInterruptType::SoundDriverVSyncOff:
      break;

    case SoftwareInterruptType::NonstdStopExecution:
      cpu.debugger().stop_execution();
      break;
    default:
      throw std::runtime_error("unimplemented swi");
  }

  // 2S + 1N
  return 3;
}

void Cpu::handle_interrupts() {
  const u32 next_pc = reg(Register::R15) - prefetch_offset() + 4;

  set_saved_program_status_for_mode(Mode::IRQ, m_current_program_status);
  change_mode(Mode::IRQ);
  set_reg(Register::R14, next_pc);

  m_current_program_status.set_irq_enabled(false);
  set_thumb(false);

  set_reg(Register::R15, 0x00000128);
}

u32 Cpu::execute() {
  if (interrupts_waiting.data() > 0 || halted) {
    return 1;
  }

  const u32 instruction = [this] {
#define NEW
#ifdef NEW
    if (const u32 pc_region = reg(Register::R15) & 0xff000000;
        m_current_memory_region != pc_region) {
      const auto [storage, _] = m_mmu->select_storage(reg(Register::R15));
      // fmt::printf("%08x %08x %08x %d\n", pc_region, m_current_memory_region,
      //            reg(Register::R15), storage.size());
      m_current_memory_region = pc_region;
      m_current_memory = storage;
      m_memory_offset = pc_region;
      // m_memory_offset = pc_region == 0 ? 0x128 : pc_region;
    }
#endif
    if (m_current_program_status.thumb_mode()) {
      static constexpr u32 nop = 0b1110'00'0'1101'0'0000'0000'000000000000;
      const u32 pc = reg(Register::R15) - 2;

#ifdef NEW
      u16 thumb_instruction;
      std::memcpy(&thumb_instruction, &m_current_memory[pc - m_memory_offset],
                  sizeof(u16));
#else
      const u16 thumb_instruction = m_mmu->at<u16>(pc);
#endif
      // fmt::printf("%08x\n", pc);

      // Long branch with link
      if ((thumb_instruction & 0xf000) == 0xf000) {
        const u32 offset = (thumb_instruction & 0x7ff);
        if (!test_bit(thumb_instruction, 11)) {
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
      return convert_thumb_to_arm(thumb_instruction);
    }

    const u32 pc = reg(Register::R15) - 4;
    // fmt::printf("%08x\n", pc);
#ifdef NEW
    u32 arm_instruction;
    std::memcpy(&arm_instruction, &m_current_memory[pc - m_memory_offset],
                sizeof(u32));
#else
    const u32 arm_instruction = m_mmu->at<u32>(pc);
#endif
    set_reg(Register::R15, pc + 4);
    return arm_instruction;
  }();

  const auto type = decode_instruction_type(instruction);

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
      fmt::printf("found instruction %08x, type %d\n", instruction,
                  static_cast<u32>(type));
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

static_assert(
    decode_instruction_type(0b0000'0011'1111'0011'0011'0000'0000'0000) ==
    InstructionType::DataProcessing);
}  // namespace tests::cpu
}  // namespace gb::advance
