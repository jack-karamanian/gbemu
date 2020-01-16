#include "gba/cpu.h"
#include <fmt/printf.h>
#include <type_traits>
#include "error_handling.h"
#include "gba/hle.h"
#include "utils.h"

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
      // cpu.reg(Register::R1));
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
                  std::exp2f((180.0F - cpu.reg(Register::R1) -
                              cpu.reg(Register::R2) / 256.0F) /
                             12.0F);
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

[[nodiscard]] bool should_execute(u32 instruction,
                                  ProgramStatus program_status) {
#if 1
  const auto condition =
      static_cast<Instruction::Condition>((instruction >> 28) & 0b1111);

  switch (condition) {
    case Instruction::Condition::EQ:
      return program_status.zero();
    case Instruction::Condition::NE:
      return !program_status.zero();
    case Instruction::Condition::CS:
      return program_status.carry();
    case Instruction::Condition::CC:
      return !program_status.carry();
    case Instruction::Condition::MI:
      return program_status.negative();
    case Instruction::Condition::PL:
      return !program_status.negative();
    case Instruction::Condition::VS:
      return program_status.overflow();
    case Instruction::Condition::VC:
      return !program_status.overflow();
    case Instruction::Condition::HI:
      return program_status.carry() && !program_status.zero();
    case Instruction::Condition::LS:
      return !program_status.carry() || program_status.zero();
    case Instruction::Condition::GE:
      return program_status.negative() == program_status.overflow();
    case Instruction::Condition::LT:
      return program_status.negative() != program_status.overflow();
    case Instruction::Condition::GT:
      return !program_status.zero() &&
             program_status.negative() == program_status.overflow();
    case Instruction::Condition::LE:
      return program_status.zero() ||
             program_status.negative() != program_status.overflow();
    case Instruction::Condition::AL:
      return true;
  }
#else
  constexpr void* table[] = {&&eq, &&ne, &&cs, &&cc,     &&mi, &&pl,
                             &&vs, &&vc, &&hi, &&ls,     &&ge, &&lt,
                             &&gt, &&le, &&al, &&invalid};

  goto* table[(instruction >> 28) & 0b1111];
eq:
  return program_status.zero();
ne:
  return !program_status.zero();
cs:
  return program_status.carry();
cc:
  return !program_status.carry();
mi:
  return program_status.negative();
pl:
  return !program_status.negative();
vs:
  return program_status.overflow();
vc:
  return !program_status.overflow();
hi:
  return program_status.carry() && !program_status.zero();
ls:
  return !program_status.carry() || program_status.zero();
ge:
  return program_status.negative() == program_status.overflow();
lt:
  return program_status.negative() != program_status.overflow();
gt:
  return !program_status.zero() &&
         program_status.negative() == program_status.overflow();
le:
  return program_status.zero() ||
         program_status.negative() != program_status.overflow();
al:
  return true;
invalid:
#endif
  throw std::runtime_error("invalid condition");
}

struct ShiftFlags {
  std::optional<u32> result;
  bool set_carry;
};

ShiftFlags compute_carry(Cpu& cpu,
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
  return {error_value, set_carry.value_or([&]() -> bool {
            switch (shift_type) {
              case ShiftType::LogicalLeft:
                return shift_amount != 0 &&
                       gb::test_bit(reg_value, 32 - shift_amount);
              case ShiftType::LogicalRight:
                return gb::test_bit(reg_value, shift_amount - 1);
              case ShiftType::ArithmeticRight:
                return gb::test_bit(reg_value, shift_amount - 1);
              case ShiftType::RotateRight:
                return gb::test_bit(reg_value, shift_amount - 1);
            }
            GB_UNREACHABLE();
          }())};
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

  const auto run_arithmetic = [&](u64 op1, u64 op2, bool write,
                                  FunctionRef<u64(u64, u64)> impl,
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
void run_shift(Cpu& cpu,
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

void multiply(Cpu& cpu,
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

void branch_and_exchange(Cpu& cpu, Register next_pc_reg) {
  const u32 reg_value = cpu.reg(next_pc_reg);

  const bool thumb_mode = gb::test_bit(reg_value, 0);
  cpu.set_thumb(thumb_mode);

  cpu.set_reg(Register::R15, reg_value);
}

}  // namespace common

using InstFunc = u32 (*)(Cpu&, u32);

template <bool Link>
constexpr u32 make_branch(Cpu& cpu, u32 instruction) {
  const bool negative = test_bit(instruction, 23);

  const bool thumb_mode = cpu.program_status().thumb_mode();
  // Convert 24 bit signed to 32 bit signed
  const s32 offset = ((instruction & 0b0111'1111'1111'1111'1111'1111)
                      << (thumb_mode ? 0 : 2)) |
                     (negative ? (thumb_mode ? 0xff800000 : (0xfe << 24)) : 0);

  const u32 next_pc = cpu.reg(Register::R15) + offset;

  if constexpr (Link) {
    cpu.set_reg(Register::R14, cpu.reg(Register::R15) - 4);
  }

  cpu.set_reg(Register::R15, next_pc);
  return 3;
}

// Operand register
constexpr Register rn(u32 instruction) noexcept {
  return static_cast<Register>((instruction >> 16) & 0xf);
}

// Destination register
constexpr Register rd(u32 instruction) noexcept {
  return static_cast<Register>((instruction >> 12) & 0xf);
}

template <bool ImmediateOperand>
auto shift_value(const Cpu& cpu, u32 instruction) -> ShiftResult {
  if constexpr (ImmediateOperand) {
    return {ShiftType::RotateRight,
            static_cast<u8>(((instruction >> 8) & 0xf) * 2),
            instruction & 0xff,
            {},
            cpu.program_status().carry()};
  }
  return compute_shift_value(instruction, cpu);
}

template <bool ImmediateOperand>
auto compute_operand2(const Cpu& cpu, u32 instruction)
    -> std::tuple<bool, u32> {
  const ShiftResult shift_result =
      shift_value<ImmediateOperand>(cpu, instruction);
  return compute_shifted_operand(shift_result);
};

template <bool ImmediateOperand, Opcode opcode, bool SetConditionCode>
constexpr u32 make_data_processing(Cpu& cpu, u32 instruction) {
  const auto shift_value = [&](const Cpu& cpu) -> ShiftResult {
    if constexpr (ImmediateOperand) {
      return {ShiftType::RotateRight,
              static_cast<u8>(((instruction >> 8) & 0xf) * 2),
              instruction & 0xff,
              {},
              cpu.program_status().carry()};
    }
    return compute_shift_value(instruction, cpu);
  };

  const auto compute_operand2 = [&](const Cpu& cpu) -> std::tuple<bool, u32> {
    const ShiftResult shift_result = shift_value(cpu);
    return compute_shifted_operand(shift_result);
  };
  const auto [shift_carry, operand2] = compute_operand2(cpu);
  const Register dest_reg = rd(instruction);

  const u32 operand1 = cpu.reg(rn(instruction)); /*&
                       // Mask off bit 1 of R15 for thumb "adr #xx" instructions
                       ~(cpu.program_status().thumb_mode() && rn(instruction) ==
                       Register::R15 && dest_reg != Register::R15 ? 0b10 : 0);
                 */

  common::data_processing<opcode, SetConditionCode>(cpu, dest_reg, operand1,
                                                    operand2, shift_carry);

  const auto cycles = [instruction]() -> Cycles {
    const bool register_shift = !ImmediateOperand && test_bit(instruction, 4);
    if (register_shift && rd(instruction) == Register::R15) {
      return 2_seq + 1_nonseq + 1_intern;
    }
    if (register_shift) {
      return 1_seq + 1_intern;
    }
    if (rd(instruction) == Register::R15) {
      return 2_seq + 1_nonseq;
    }
    return 1_seq;
  };

  return cycles().sum();
}
namespace multiply {

namespace detail {
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
}  // namespace detail

template <bool Accumulate, bool SetConditionCode>
u32 make_multiply(Cpu& cpu, u32 instruction) {
  const auto dest_register = [&]() {
    return static_cast<Register>((instruction >> 16) & 0xf);
  };

  const auto lhs_register = [&]() {
    return static_cast<Register>(instruction & 0xf);
  };

  const auto rhs_register = [&]() {
    return static_cast<Register>((instruction >> 8) & 0xf);
  };

  const auto accumulate_register = [&]() {
    return static_cast<Register>((instruction >> 12) & 0xf);
  };

  const u32 rhs_operand = cpu.reg(rhs_register());
  const u32 res =
      static_cast<u32>(static_cast<u64>(cpu.reg(lhs_register())) *
                           static_cast<u64>(rhs_operand) +
                       (Accumulate ? cpu.reg(accumulate_register()) : 0));

  cpu.set_reg(dest_register(), res);

  if (SetConditionCode) {
    cpu.set_zero(res == 0);
    cpu.set_negative(gb::test_bit(res, 31));
  }

  return detail::multiply_cycles(rhs_operand, Accumulate).sum();
}

template <bool IsSigned, bool Accumulate, bool SetConditionCode>
u32 make_multiply_long(Cpu& cpu, u32 instruction) {
  const auto lhs_register = [&]() {
    return static_cast<Register>(instruction & 0xf);
  };

  const auto rhs_register = [&]() {
    return static_cast<Register>((instruction >> 8) & 0xf);
  };

  const auto dest_register_high = [instruction]() {
    return static_cast<Register>((instruction >> 16) & 0xf);
  }();

  const auto dest_register_low = [instruction]() {
    return static_cast<Register>((instruction >> 12) & 0xf);
  }();

  const u32 lhs = cpu.reg(lhs_register());
  const u32 rhs = cpu.reg(rhs_register());

  const auto accumulate_value = [&]() -> u64 {
    if (!Accumulate) {
      return 0;
    }

    return (static_cast<u64>(cpu.reg(dest_register_high)) << 32) |
           static_cast<u64>(cpu.reg(dest_register_low));
  }();

  const u64 res = (IsSigned ? detail::multiply<s64>(static_cast<s32>(lhs),
                                                    static_cast<s32>(rhs))
                            : detail::multiply<u64>(lhs, rhs)) +
                  accumulate_value;

  cpu.set_reg(dest_register_high, (res & 0xffffffff00000000) >> 32);
  cpu.set_reg(dest_register_low, res & 0xffffffff);

  if (SetConditionCode) {
    cpu.set_zero(res == 0);
    cpu.set_negative(gb::test_bit(res, 63));
  }
  return detail::multiply_long_cycles(rhs, Accumulate, IsSigned).sum();
}
}  // namespace multiply

constexpr void run_write_back(Cpu& cpu, u32 addr, u32 instruction) {
  const Register base_register = rn(instruction);
  cpu.set_reg(base_register, addr);
}

template <bool AddOffsetToBase>
[[nodiscard]] constexpr u32 calculate_addr(u32 base_value, u32 offset) {
  const u32 addr = AddOffsetToBase ? offset + base_value : base_value - offset;
  return addr;
}

template <bool Preindex, bool AddOffsetToBase, bool WriteBack>
constexpr static u32 select_addr(Cpu& cpu,
                                 u32 base_value,
                                 u32 offset,
                                 u32 instruction) {
  if constexpr (Preindex) {
    const u32 addr = calculate_addr<AddOffsetToBase>(base_value, offset);

    if constexpr (WriteBack) {
      run_write_back(cpu, addr, instruction);
    }
    return addr;
  }
  return base_value;
}

template <typename T>
constexpr void run_load(Cpu& cpu, u32 addr, u32 instruction) {
  const Register dest_reg = rd(instruction);

  if constexpr (std::is_signed_v<T>) {
    cpu.set_reg(dest_reg, static_cast<s32>(cpu.mmu()->at<T>(addr)));
  } else {
    cpu.set_reg(dest_reg, cpu.mmu()->at<T>(addr));
  }
}

constexpr Cycles load_store_cycles(Register dest_reg, bool load) {
  if (!load) {
    return 2_nonseq;
  }

  // Load
  return dest_reg == Register::R15 ? (2_seq + 2_nonseq + 1_intern)
                                   : (1_seq + 1_nonseq + 1_intern);
}
enum class TransferType : u32 {
  Swp = 0,
  UnsignedHalfword,
  SignedByte,
  SignedHalfword,
};

template <bool Preindex,
          bool AddOffsetToBase,
          bool ImmediateOffset,
          bool WriteBack,
          bool Load,
          TransferType transfer_type>
u32 make_halfword_data_transfer(Cpu& cpu, u32 instruction) {
  Mmu& mmu = *cpu.mmu();
  const u32 offset = [&cpu, instruction]() {
    if (ImmediateOffset) {
      return ((instruction & 0xf00) >> 4) | (instruction & 0xf);
    }

    const auto offset_reg = static_cast<Register>(instruction & 0xf);
    return cpu.reg(offset_reg);
  }();  // offset_value(cpu);
  const u32 base_value = cpu.reg(rn(instruction));
  const u32 addr = select_addr<Preindex, AddOffsetToBase, WriteBack>(
      cpu, base_value, offset, instruction);

  // const auto transfer_type =
  //    static_cast<TransferType>((instruction & 0x60) >> 5);

  const Register src_or_dest_reg = rd(instruction);

  if constexpr (Load) {
    switch (transfer_type) {
      case TransferType::Swp:
        break;
      case TransferType::UnsignedHalfword:
        run_load<u16>(cpu, addr, instruction);
        break;
      case TransferType::SignedByte: {
        run_load<s8>(cpu, addr, instruction);
        break;
      }
      case TransferType::SignedHalfword: {
        run_load<s16>(cpu, addr, instruction);
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

  if constexpr (!Preindex) {
    const u32 writeback_addr =
        calculate_addr<AddOffsetToBase>(base_value, offset);
    run_write_back(cpu, writeback_addr, instruction);
  }

  return mmu.wait_cycles(addr, load_store_cycles(rd(instruction), Load));
}

template <bool ByteSwap>
u32 make_single_data_swap(Cpu& cpu, u32 instruction) {
  using T = std::conditional_t<ByteSwap, u8, u32>;
  Mmu& mmu = *cpu.mmu();

  constexpr Cycles cycles = 1_seq + 2_nonseq + 1_intern;

  const u32 base_value = cpu.reg(rn(instruction));

  T mem_value = mmu.at<T>(base_value);
  u32 reg_value = cpu.reg(static_cast<Register>(instruction & 0xf));

  if constexpr (std::is_same_v<T, u8>) {
    const u32 shift_amount = (base_value % 4) * 8;
    const u32 mask = 0xff << shift_amount;
    const u8 byte_value =
        static_cast<u8>(((reg_value & mask) >> shift_amount) & 0xff);
    mmu.set(base_value, byte_value);
  } else {
    mmu.set(base_value, reg_value);
  }
  cpu.set_reg(rd(instruction), mem_value);

  return mmu.wait_cycles(base_value, cycles);
}

template <bool ImmediateOffset,
          bool Preindex,
          bool AddOffsetToBase,
          bool WordTransfer,
          bool WriteBack,
          bool Load>
u32 make_single_data_transfer(Cpu& cpu, u32 instruction) {
  Mmu& mmu = *cpu.mmu();
  const auto calculate_offset = [&]() -> u32 {
    if constexpr (ImmediateOffset) {
      return instruction & 0xfff;
    }

    const ShiftResult shift_result = compute_shift_value(instruction, cpu);
    const auto [set_carry, offset] = compute_shifted_operand(shift_result);

    return offset;
  };

  const Register base_register = rn(instruction);
  const u32 base_value = cpu.reg(base_register); /* &
                         // Set bit 1 of R15 to 0 when in thumb mode
                         ~(cpu.program_status().thumb_mode() && base_register ==
                         Register::R15 ? 0b10 : 0);
                   */

  const u32 offset = calculate_offset();

  // The address to be used in the transfer
  const auto [aligned_addr, raw_addr,
              rotate_amount] = [&]() -> std::tuple<u32, u32, u32> {
    const u32 raw_address = select_addr<Preindex, AddOffsetToBase, WriteBack>(

        cpu, base_value, offset, instruction);
    return {raw_address & ~0b11, raw_address, (raw_address & 0b11) * 8};
  }();

  if constexpr (!Preindex) {
    const u32 writeback_addr =
        calculate_addr<AddOffsetToBase>(base_value, offset);
    run_write_back(cpu, writeback_addr, instruction);
  }

  // const bool word_transfer = word();
  const auto dest = rd(instruction);

  if constexpr (Load) {
    if constexpr (WordTransfer) {
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
    const u32 stored_value = cpu.reg(dest);
    if constexpr (WordTransfer) {
      // Store a word
      mmu.set(aligned_addr, stored_value);
    } else {
      // Store a byte
      mmu.set(raw_addr, static_cast<u8>(stored_value & 0xff));
    }
  }
  const auto cycles = [&] {
    // Store
    if constexpr (!Load) {
      return 2_nonseq;
    }

    // Load
    return dest == Register::R15 ? (2_seq + 2_nonseq + 1_intern)
                                 : (1_seq + 1_nonseq + 1_intern);
  }();

  return mmu.wait_cycles(aligned_addr, cycles);
}

template <bool ImmediateOperand, bool UseSpsrDest, bool ToStatus>
u32 make_status_transfer(Cpu& cpu, u32 instruction) {
  if constexpr (ToStatus) {
    u32 mask = 0;

    mask |= test_bit(instruction, 19) ? 0xff000000 : 0;
    mask |= test_bit(instruction, 18) ? 0x00ff0000 : 0;
    mask |= test_bit(instruction, 17) ? 0x0000ff00 : 0;
    mask |= test_bit(instruction, 16) ? 0x000000ff : 0;
    const auto [_, operand2] =
        compute_operand2<ImmediateOperand>(cpu, instruction);
    const ProgramStatus next_program_status{operand2 & mask};
    if constexpr (UseSpsrDest) {
      cpu.set_saved_program_status(next_program_status);
    } else {
      cpu.set_program_status(next_program_status);
    }
  } else {
    cpu.set_reg(rd(instruction), UseSpsrDest ? cpu.saved_program_status().data()
                                             : cpu.program_status().data());
  }
  return 1;
}

template <bool Preindex,
          bool AddOffsetToBase,
          bool LoadPsrAndUserMode,
          bool WriteBack,
          bool Load>
u32 make_block_data_transfer(Cpu& cpu, u32 instruction) {
  const auto register_list =
      [instruction]() -> std::tuple<std::array<Register, 16>, int> {
    std::array<Register, 16> registers{};
    int end = 0;

    const auto append_register = [&](int i) {
      if (test_bit(instruction, i)) {
        registers[end++] = static_cast<Register>(i);
      }
    };

    if constexpr (!AddOffsetToBase) {
      for (int i = 15; i >= 0; --i) {
        append_register(i);
      }
    } else {
      for (int i = 0; i < 16; ++i) {
        append_register(i);
      }
    }
    return {registers, end};
  };
  const Mode current_mode = cpu.program_status().mode();
  if (LoadPsrAndUserMode) {
    cpu.change_mode(Mode::User);
  }

  const auto operand_reg = rn(instruction);
  u32 offset = cpu.reg(operand_reg);

  const auto change_offset = AddOffsetToBase ? [](u32 val) { return val + 4; }
                                             : [](u32 val) { return val - 4; };
  const auto [registers, registers_end] = register_list();
  const nonstd::span<const Register> registers_span{registers.data(),
                                                    registers_end};

  u32 addr_cycles = 0;
  const u32 absolute_offset = 4 * registers_end;
  const u32 final_offset =
      AddOffsetToBase ? (offset + absolute_offset) : (offset - absolute_offset);

  const bool regs_has_base =
      std::find(registers_span.begin(), registers_span.end(), operand_reg) !=
      registers_span.end();

  constexpr bool load_register = Load;
  constexpr bool write_back_base = WriteBack;
  constexpr bool preindex_base = Preindex;
  constexpr bool add_offset = AddOffsetToBase;

  if (write_back_base) {
    if (load_register) {
      cpu.set_reg(operand_reg, final_offset);
    } else if (regs_has_base &&
               ((!add_offset && registers[registers_end - 1] != operand_reg) ||
                (add_offset && registers[0] != operand_reg))) {
      cpu.set_reg(operand_reg, final_offset);
    }
  }

  for (const Register reg : registers_span) {
    if (preindex_base) {
      offset = change_offset(offset);
    }

    addr_cycles += cpu.mmu()->wait_cycles(offset, 1_seq);
    if (load_register) {
      cpu.set_reg(reg, cpu.mmu()->at<u32>(offset));
    } else {
      cpu.mmu()->set(offset, cpu.reg(reg));
    }

    if (!preindex_base) {
      offset = change_offset(offset);
    }
  }

  if (!load_register && write_back_base) {
    cpu.set_reg(operand_reg, final_offset);
  }

  if (LoadPsrAndUserMode) {
    cpu.change_mode(current_mode);
  }

  return addr_cycles + (1_nonseq + 1_intern).sum();
}

namespace arm {
u32 branch_and_exchange(Cpu& cpu, u32 instruction) {
  common::branch_and_exchange(cpu, static_cast<Register>(instruction & 0xf));
  return 3;
};
u32 software_interrupt(Cpu& cpu, u32 instruction) {
  return SoftwareInterrupt{instruction}.execute(cpu);
}

[[noreturn]] u32 invalid_instruction([[maybe_unused]] Cpu& cpu,
                                     [[maybe_unused]] u32 instruction) {
  throw std::runtime_error("invalid instruction");
}
}  // namespace arm

namespace thumb {
template <ShiftType shift_type, u32 shift_amount>
u32 move_shifted_register(Cpu& cpu, u16 instruction) {
  const auto dest_reg = static_cast<Register>(instruction & 0b111);
  const auto src_reg = static_cast<Register>((instruction >> 3) & 0b111);

  const u32 value = cpu.reg(src_reg);
  common::run_shift(cpu, shift_type, dest_reg, value, shift_amount, false);

  return 1;
}

template <bool ImmediateOperand, bool Subtract, u32 RegisterOrValue>
u32 add_subtract(Cpu& cpu, u16 instruction) {
  const auto dest_reg = static_cast<Register>(instruction & 0b111);
  const auto src_reg = static_cast<Register>((instruction >> 3) & 0b111);

  const u32 value = ImmediateOperand
                        ? RegisterOrValue
                        : cpu.reg(static_cast<Register>(RegisterOrValue));

  common::data_processing<Subtract ? Opcode::Sub : Opcode::Add, true>(
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
    constexpr auto translated_opcode = opcode_table[opcode];
    static_assert(translated_opcode != Opcode::Mov);

    common::data_processing<translated_opcode, true>(
        cpu, dest_reg,
        cpu.reg(translated_opcode == Opcode::Rsb ? src_reg : dest_reg),
        translated_opcode == Opcode::Rsb ? 0 : cpu.reg(src_reg),
        cpu.program_status().carry());
  }

  return 3;
}

template <int opcode, bool hi_dest, bool hi_src>
u32 hi_register_operation(Cpu& cpu, u16 instruction) {
  static constexpr std::array opcode_table = {Opcode::Add, Opcode::Cmp,
                                              Opcode::Mov};

  const auto dest_reg =
      static_cast<Register>((instruction & 0b111) + hi_dest * 8);
  const auto src_reg =
      static_cast<Register>(((instruction >> 3) & 0b111) + hi_src * 8);

  if constexpr (opcode == 0b11) {
    // BX
    common::branch_and_exchange(cpu, src_reg);
    return 3;
  } else {
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

  constexpr bool word_transfer = sizeof(Type) == 4;
  const u32 addr = cpu.reg(base_reg) + offset;

  const u32 aligned_addr = addr & ~0b11;
  const u32 rotate_amount = (addr & 0b11) * 8;
  const u32 resolved_addr = word_transfer ? aligned_addr : addr;
  if constexpr (load) {
    const auto value = cpu.mmu()->at<Type>(resolved_addr);
    cpu.set_reg(
        dest_reg,
        static_cast<std::conditional_t<std::is_signed_v<Type>, s32, u32>>(
            word_transfer ? rotate_right(value, rotate_amount) : value));
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
    cpu.set_reg(dest_reg, cpu.mmu()->at<u32>(addr));
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

template <bool load, Register base_reg>
u32 multiple_load_store(Cpu& cpu, u16 instruction) {
  u32 base = cpu.reg(base_reg);
  u32 cycles = 0;

  for (int i = 0; i < 8; ++i) {
    if (test_bit(instruction, i)) {
      const auto reg = static_cast<Register>(i);
      if constexpr (load) {
        // LDMIA
        cpu.set_reg(reg, cpu.mmu()->at<u32>(base));
      } else {
        // STMIA
        cpu.mmu()->set(base, cpu.reg(reg));
      }
      cycles += cpu.mmu()->wait_cycles(
          base, load_store_cycles(load ? reg : base_reg, load));

      base += 4;
    }
  }

  cpu.set_reg(base_reg, base);

  return cycles;
}

u32 conditional_branch(Cpu& cpu, u16 instruction) {
  // The condition is handled externally
  const u32 offset = static_cast<s32>(static_cast<s8>(instruction & 0xff)) << 1;

  cpu.set_reg(Register::R15, cpu.reg(Register::R15) + offset);
  return 1;
}

u32 software_interrupt(Cpu& cpu, u16 instruction) {
  return SoftwareInterrupt{static_cast<u32>(instruction & 0xff)}.execute(cpu);
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

using ThumbInstFunc = u32 (*)(Cpu&, u16);
static constexpr std::array<ThumbInstFunc, 1024> thumb_lookup_table = [] {
  std::array<ThumbInstFunc, 1024> res{};
  for_static<1024>([&](auto i) {
    const u16 instruction = i << 6;
    switch ((instruction >> 13) & 0b111) {
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
        // Move/compare/add/subtract immediate
        constexpr auto opcode = [&]() -> Opcode {
          switch ((instruction >> 11) & 0b11) {
            case 0:
              return Opcode::Mov;
            case 1:
              return Opcode::Cmp;
            case 2:
              return Opcode::Add;
            case 3:
              return Opcode::Sub;
          }
          throw "Not reachable";
        }();
        const auto dest_reg = static_cast<Register>((instruction >> 8) & 0b111);

        res[i] = thumb::move_compare_add_subtract_immediate<opcode, dest_reg>;
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
              constexpr auto decode_type = [] {
                if constexpr (opcode <= 0b01) {
                  return u16{};
                } else if constexpr (opcode == 0b10) {
                  return s8{};
                } else {
                  return s16{};
                }
              };

              using Type = decltype(decode_type());
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
  });
  return res;
}();

u32 Cpu::execute() {
  if (interrupts_waiting.data() > 0 || halted) {
    return 1;
  }

  static constexpr std::array<InstFunc, 4096> func_lookup_table1 = [] {
    std::array<InstFunc, 4096> res{};

    for_static<4096>([&](auto i) {
      constexpr auto top_index = (i >> 8) & 0b1111;
      // Bits 4-7
      constexpr auto lower_bits = i & 0b1111;
      // Bits 20-23
      constexpr auto upper_bits = (i >> 4) & 0b1111;

      switch (top_index) {
        case 0: {
          switch (lower_bits) {
            case 0b1001:
              if constexpr (test_bit(upper_bits, 3)) {
                res[i] = multiply::make_multiply_long<test_bit(upper_bits, 2),
                                                      test_bit(upper_bits, 1),
                                                      test_bit(upper_bits, 0)>;
              } else {
                res[i] = multiply::make_multiply<test_bit(upper_bits, 1),
                                                 test_bit(upper_bits, 0)>;
              }
              break;
            case 0b1011:
            case 0b1101:
            case 0b1111:
              res[i] = make_halfword_data_transfer<
                  false, test_bit(upper_bits, 3), test_bit(upper_bits, 2),
                  test_bit(upper_bits, 1), test_bit(upper_bits, 0),
                  static_cast<TransferType>((lower_bits >> 1) & 0b11)>;
              // HalfwordDataTransfer
              break;
            default:
              res[i] =
                  make_data_processing<false,
                                       static_cast<Opcode>(upper_bits >> 1),
                                       test_bit(upper_bits, 0)>;
              break;
          }
          break;
        }
        case 1: {
          const bool set_condition_code = test_bit(upper_bits, 0);
          const u32 opcode = 0b1000 | (upper_bits >> 1);
          constexpr auto data_processing_instruction =
              make_data_processing<false, static_cast<Opcode>(opcode),
                                   test_bit(upper_bits, 0)>;
          switch (lower_bits) {
            case 0b1001:
              // Swap
              res[i] = make_single_data_swap<test_bit(upper_bits, 2)>;
              break;
            case 0b1011:
            case 0b1101:
            case 0b1111:
              // HalfwordDataTransfer
              res[i] = make_halfword_data_transfer<
                  true, test_bit(upper_bits, 3), test_bit(upper_bits, 2),
                  test_bit(upper_bits, 1), test_bit(upper_bits, 0),
                  static_cast<TransferType>((lower_bits >> 1) & 0b11)>;
              break;
            case 0b0001:
              if (!set_condition_code && opcode == 0b1001) {
                res[i] = arm::branch_and_exchange;
              } else {
                res[i] = data_processing_instruction;
              }
              break;
            default:
              if (!set_condition_code && opcode >= 0b1000 && opcode <= 0b1011) {
                res[i] = make_status_transfer<false, test_bit(upper_bits, 2),
                                              test_bit(upper_bits, 1)>;
              } else {
                res[i] = data_processing_instruction;
              }
              break;
          }
          break;
        }
        case 2: {
          res[i] =
              make_data_processing<true, static_cast<Opcode>(upper_bits >> 1),
                                   test_bit(upper_bits, 0)>;
          break;
        }
        case 3: {
          constexpr bool set_condition_code = test_bit(upper_bits, 0);
          constexpr u32 opcode = 0b1000 | (upper_bits >> 1);
          if constexpr (!set_condition_code && opcode >= 0b1000 &&
                        opcode <= 0b1011) {
            res[i] = make_status_transfer<true, test_bit(upper_bits, 2),
                                          test_bit(upper_bits, 1)>;
          } else {
            res[i] = make_data_processing<
                true, static_cast<Opcode>(0b1000 | (upper_bits >> 1)),
                test_bit(upper_bits, 0)>;
          }
          break;
        }
        case 4:
        case 5:
        case 6:
        case 7: {
          constexpr bool immediate_offset = !test_bit(top_index, 1);
          constexpr bool preindex = test_bit(top_index, 0);
          constexpr bool add_offset_to_base = test_bit(upper_bits, 3);
          constexpr bool word_transfer = !test_bit(upper_bits, 2);
          constexpr bool write_back = test_bit(upper_bits, 1);
          constexpr bool load = test_bit(upper_bits, 0);

          res[i] = make_single_data_transfer<immediate_offset, preindex,
                                             add_offset_to_base, word_transfer,
                                             write_back, load>;
          break;
        }
        case 8:
        case 9: {
          constexpr bool preindex = test_bit(top_index, 0);
          constexpr bool add_offset_to_base = test_bit(upper_bits, 3);
          constexpr bool psr_and_user_mode = test_bit(upper_bits, 2);
          constexpr bool write_back = test_bit(upper_bits, 1);
          constexpr bool load = test_bit(upper_bits, 0);
          res[i] =
              make_block_data_transfer<preindex, add_offset_to_base,
                                       psr_and_user_mode, write_back, load>;
          break;
        }
        case 10:
          res[i] = make_branch<false>;
          break;
        case 11:
          res[i] = make_branch<true>;
          break;

        case 15:
          res[i] = arm::software_interrupt;
          break;

        default:
          res[i] = arm::invalid_instruction;
          break;
      }
    });

    return res;
  }();

  const u32 instruction = [this]() -> u32 {
    if (const u32 pc_region = m_regs[15] & 0xff000000;
        m_current_memory_region != pc_region) {
      const auto [storage, _] = m_mmu->select_storage(reg(Register::R15));
      // fmt::printf("%08x %08x %08x %d\n", pc_region,
      // m_current_memory_region,
      //            reg(Register::R15), storage.size());
      m_current_memory_region = pc_region;
      m_current_memory = storage;
      m_memory_offset = pc_region;
      // m_memory_offset = pc_region == 0 ? 0x128 : pc_region;
    }
    if (m_current_program_status.thumb_mode()) {
      static constexpr u32 nop = 0b1110'00'0'1101'0'0000'0000'000000000000;
      const u32 pc = reg(Register::R15) - 2;

      u16 thumb_instruction;
      std::memcpy(&thumb_instruction, &m_current_memory[pc - m_memory_offset],
                  sizeof(u16));
      // fmt::printf("%08x\n", pc);

      // Long branch with link
#if 0
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
#endif
      set_reg(Register::R15, pc + 2);

      // if (thumb_lookup_table[(thumb_instruction >> 6) & 0x3ff] != nullptr) {
      return thumb_instruction;
      //}
      // return convert_thumb_to_arm(thumb_instruction);
    }

    const u32 pc = reg(Register::R15) - 4;
    // fmt::printf("%08x\n", pc);
    u32 arm_instruction;
    std::memcpy(&arm_instruction, &m_current_memory[pc - m_memory_offset],
                sizeof(u32));
    set_reg(Register::R15, pc + 4);
    return arm_instruction;
  }();

  const auto execute_arm_instruction = [&](u32 instruction) -> u32 {
    const auto inst_func =
        func_lookup_table1[(((instruction >> 20) & 0xff) << 4) |
                           ((instruction >> 4) & 0b1111)];

    if (should_execute(instruction, m_current_program_status)) {
      return inst_func(*this, instruction);
    }
    return 0;
  };

  if (m_current_program_status.thumb_mode()) {
    if (const u32 condition = (instruction >> 8) & 0b1111;
        (instruction & 0b1101'0000'0000'0000) == 0b1101'0000'0000'0000 &&
        ((instruction >> 12) != 0b1111) && condition != 0b1111) {
      if (should_execute(condition << 28, m_current_program_status)) {
        return thumb::conditional_branch(*this, instruction);
      }
      return 0;
    }
    const auto inst_func = thumb_lookup_table[(instruction >> 6) & 0x3ff];
    // if ((instruction & 0b1111'1111'1111'1111'0000'0000'0000'0000) == 0 &&
    //    inst_func != nullptr) {
    return inst_func(*this, instruction);
  }
  return execute_arm_instruction(instruction);
}  // namespace gb::advance
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

#if 0
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
#endif

static_assert(
    decode_instruction_type(0b0000'0011'1111'0011'0011'0000'0000'0000) ==
    InstructionType::DataProcessing);
}  // namespace tests::cpu
}  // namespace gb::advance
