#include "gba/cpu.h"
#include <fmt/printf.h>
#include <type_traits>
#include "error_handling.h"
#include "gba/common_instructions.h"
#include "gba/hle.h"
#include "gba/thumb_instructions.h"
#include "utils.h"

namespace gb::advance {

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
  NonstdPrintInt = 0xfe,
};

u32 execute_software_interrupt(Cpu& cpu, u32 instruction) {
  using namespace gb::advance::hle::bios;

  const auto interrupt_type = static_cast<SoftwareInterruptType>(
      (instruction & 0x00ffffff) >>
      (cpu.program_status().thumb_mode() ? 0 : 16));

  switch (interrupt_type) {
    case SoftwareInterruptType::SoftReset:
      cpu.soft_reset();
      break;
    case SoftwareInterruptType::RegisterRamReset:
      break;
    case SoftwareInterruptType::Halt:
      cpu.halted = true;
      break;
    case SoftwareInterruptType::VBlankIntrWait: {
      const u32 next_pc = cpu.reg(Register::R15) - cpu.prefetch_offset();
      cpu.set_saved_program_status_for_mode(Mode::Supervisor,
                                            cpu.program_status());

      cpu.change_mode(Mode::Supervisor);
      cpu.set_reg(Register::R14, next_pc);
      cpu.set_thumb(false);
      cpu.set_reg(Register::R15, Mmu::VBlankIntrWaitAddr);
      break;
    }
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
      cpu.set_reg(Register::R0, static_cast<u32>(arctan2(x, y)) & 0xffff);
      break;
    }
    case SoftwareInterruptType::CpuSet:
      cpu_set(*cpu.mmu(), cpu.reg(Register::R0), cpu.reg(Register::R1),
              cpu.reg(Register::R2));
      break;
    case SoftwareInterruptType::CpuFastSet:
      cpu_fast_set(*cpu.mmu(), cpu.reg(Register::R0), cpu.reg(Register::R1),
                   cpu.reg(Register::R2));
      break;
    case SoftwareInterruptType::BgAffineSet:
      bg_affine_set(*cpu.mmu(), cpu.reg(Register::R0), cpu.reg(Register::R1),
                    cpu.reg(Register::R2));
      break;
    case SoftwareInterruptType::ObjAffineSet: {
      const u32 src = cpu.reg(Register::R0);

      const u32 dest = cpu.reg(Register::R1);

      const u32 count = cpu.reg(Register::R2);
      const u32 stride = cpu.reg(Register::R3);

      obj_affine_set(*cpu.mmu(), src, dest, count, stride);
    } break;
    case SoftwareInterruptType::Lz77Wram: {
      const u32 src = cpu.reg(Register::R0);
      if (memory_region(src) == 0) {
        break;
      }
      const auto [source_storage, source_addr] = cpu.mmu()->select_storage(src);
      const auto [dest_storage, dest_addr] =
          cpu.mmu()->select_storage(cpu.reg(Register::R1));
      lz77_decompress(source_storage.subspan(source_addr),
                      dest_storage.subspan(dest_addr), 1);
      break;
    }
    case SoftwareInterruptType::Lz77Vram: {
      const auto [source_storage, source_addr] =
          cpu.mmu()->select_storage(cpu.reg(Register::R0));
      const auto [dest_storage, dest_addr] =
          cpu.mmu()->select_storage(cpu.reg(Register::R1) & ~1);
      lz77_decompress(source_storage.subspan(source_addr),
                      dest_storage.subspan(dest_addr), 2);
      break;
    }
    case SoftwareInterruptType::MidiKeyToFreq: {
      // From mgba
      auto freq = cpu.mmu()->at<u32>(cpu.reg(Register::R0) + 4) /
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
    case SoftwareInterruptType::NonstdPrintInt:
      fmt::printf("%d\n", cpu.reg(Register::R0));
      break;
    default:
      throw std::runtime_error(fmt::format("unimplemented swi {}",
                                           static_cast<u32>(interrupt_type)));
  }

  // 2S + 1N
  return 3;
}

void Cpu::handle_interrupts() {
#if 0
  const u32 next_pc = reg(Register::R15) - prefetch_offset() + 4;

  set_saved_program_status_for_mode(Mode::IRQ, m_current_program_status);
  change_mode(Mode::IRQ);
  set_reg(Register::R14, next_pc);

  m_current_program_status.set_irq_enabled(false);
  set_thumb(false);

  set_reg(Register::R15, 0x00000128);
#else
  if (const auto data = interrupts_waiting.data();
      (interrupts_requested.data() & data) != 0) {
    interrupts_waiting.set_data(0);
  }
  if ((interrupts_enabled.data() & interrupts_requested.data()) != 0) {
    halted = false;
    if (gb::test_bit(ime, 0) && program_status().irq_enabled()) {
      const u32 next_pc = reg(Register::R15) - prefetch_offset() + 4;

      set_saved_program_status_for_mode(Mode::IRQ, m_current_program_status);
      change_mode(Mode::IRQ);
      set_reg(Register::R14, next_pc);

      m_current_program_status.set_irq_enabled(false);
      set_thumb(false);

      set_reg(Register::R15, 0x00000128);
    }
  }
#endif
}

[[nodiscard]] bool should_execute(u32 instruction,
                                  ProgramStatus program_status) {
  const auto condition = static_cast<Condition>((instruction >> 28) & 0b1111);

  switch (condition) {
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

using InstFunc = u32 (*)(Cpu&, u32);

template <bool link>
constexpr u32 make_branch(Cpu& cpu, u32 instruction) {
  const bool negative = test_bit(instruction, 23);

  const bool thumb_mode = cpu.program_status().thumb_mode();
  // Convert 24 bit signed to 32 bit signed
  const s32 offset = ((instruction & 0b0111'1111'1111'1111'1111'1111)
                      << (thumb_mode ? 0 : 2)) |
                     (negative ? (thumb_mode ? 0xff800000 : (0xfe << 24)) : 0);

  const u32 next_pc = cpu.reg(Register::R15) + offset;

  if constexpr (link) {
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

template <bool immediate_operand>
constexpr auto shift_value(const Cpu& cpu, u32 instruction) -> ShiftResult {
  if constexpr (immediate_operand) {
    const auto shift_amount = static_cast<u8>(((instruction >> 8) & 0xf) * 2);
    const auto shift_operand = instruction & 0xff;
    return {ShiftType::RotateRight,
            shift_amount,
            shift_operand,
            {},
            shift_amount == 0 ? false
                              : gb::test_bit(shift_operand, shift_amount - 1)};
  }
  return compute_shift_value(instruction, cpu);
}

template <bool immediate_operand>
auto compute_operand2(const Cpu& cpu, u32 instruction)
    -> std::tuple<bool, u32> {
  const ShiftResult shift_result =
      shift_value<immediate_operand>(cpu, instruction);
  return compute_shifted_operand(shift_result);
}

namespace arm {
template <bool accumulate, bool set_condition_code>
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
                       (accumulate ? cpu.reg(accumulate_register()) : 0));

  cpu.set_reg(dest_register(), res);

  if (set_condition_code) {
    cpu.set_zero(res == 0);
    cpu.set_negative(gb::test_bit(res, 31));
  }

  return multiply::detail::multiply_cycles(rhs_operand, accumulate).sum();
}

template <bool is_signed, bool accumulate, bool set_condition_code>
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
    if (!accumulate) {
      return 0;
    }

    return (static_cast<u64>(cpu.reg(dest_register_high)) << 32) |
           static_cast<u64>(cpu.reg(dest_register_low));
  }();

  const u64 res = (is_signed ? multiply::detail::multiply<s64>(
                                   static_cast<s32>(lhs), static_cast<s32>(rhs))
                             : multiply::detail::multiply<u64>(lhs, rhs)) +
                  accumulate_value;

  cpu.set_reg(dest_register_high, (res & 0xffffffff00000000) >> 32);
  cpu.set_reg(dest_register_low, res & 0xffffffff);

  if (set_condition_code) {
    cpu.set_zero(res == 0);
    cpu.set_negative(gb::test_bit(res, 63));
  }
  return multiply::detail::multiply_long_cycles(rhs, accumulate, is_signed)
      .sum();
}
template <bool immediate_operand, Opcode opcode, bool set_condition_code>
constexpr u32 make_data_processing(Cpu& cpu, u32 instruction) {
  const auto [shift_carry, operand2] =
      compute_operand2<immediate_operand>(cpu, instruction);

  const Register dest_reg = rd(instruction);

  const u32 operand1 = cpu.reg(rn(instruction)) +
                       (!immediate_operand && test_bit(instruction, 4) &&
                                rn(instruction) == Register::R15
                            ? 4
                            : 0);

  common::data_processing<opcode, set_condition_code>(cpu, dest_reg, operand1,
                                                      operand2, shift_carry);

  const auto cycles = [instruction]() -> Cycles {
    const bool register_shift = !immediate_operand && test_bit(instruction, 4);
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
constexpr void run_write_back(Cpu& cpu, u32 addr, u32 instruction) {
  const Register base_register = rn(instruction);
  cpu.set_reg(base_register, addr);
}

template <bool add_offset_to_base>
[[nodiscard]] constexpr u32 calculate_addr(u32 base_value, u32 offset) {
  const u32 addr =
      add_offset_to_base ? offset + base_value : base_value - offset;
  return addr;
}

template <bool preindex, bool add_offset_to_base, bool write_back>
constexpr static u32 select_addr(Cpu& cpu,
                                 u32 base_value,
                                 u32 offset,
                                 u32 instruction) {
  if constexpr (preindex) {
    const u32 addr = calculate_addr<add_offset_to_base>(base_value, offset);

    if constexpr (write_back) {
      run_write_back(cpu, addr, instruction);
    }
    return addr;
  } else {
    static_cast<void>(instruction);
    static_cast<void>(offset);
  }

  return base_value;
}

enum class TransferType : u32 {
  Swp = 0,
  UnsignedHalfword,
  SignedByte,
  SignedHalfword,
};

template <bool preindex,
          bool add_offset_to_base,
          bool immediate_offset,
          bool write_back,
          bool load,
          TransferType transfer_type>
u32 make_halfword_data_transfer(Cpu& cpu, u32 instruction) {
  Mmu& mmu = *cpu.mmu();
  const Register src_or_dest_reg = rd(instruction);
  const u32 original_dest_value = cpu.reg(src_or_dest_reg);

  const u32 offset = [&cpu, instruction]() {
    if (immediate_offset) {
      return ((instruction & 0xf00) >> 4) | (instruction & 0xf);
    }

    const auto offset_reg = static_cast<Register>(instruction & 0xf);
    return cpu.reg(offset_reg);
  }();  // offset_value(cpu);
  const Register base_reg = rn(instruction);
  const u32 base_value = cpu.reg(base_reg);
  const u32 addr = select_addr<preindex, add_offset_to_base, write_back>(
      cpu, base_value, offset, instruction);

  // const auto transfer_type =
  //    static_cast<TransferType>((instruction & 0x60) >> 5);

  const auto get_transfer_type = [] {
    if constexpr (transfer_type == TransferType::UnsignedHalfword) {
      return u16{};
    } else if constexpr (transfer_type == TransferType::SignedByte) {
      return s8{};
    } else if constexpr (transfer_type == TransferType::SignedHalfword) {
      return s16{};
    }
  };

  using Type = decltype(get_transfer_type());
  static_assert(!std::is_same_v<Type, void>, "can't SWP");

  if constexpr (load) {
    run_load<Type>(cpu, addr, src_or_dest_reg);
  } else {
    const auto aligned_addr = addr & ~0b01;
    // Store
    const auto value = ((write_back || !preindex) && base_reg == src_or_dest_reg
                            ? original_dest_value
                            : cpu.reg(src_or_dest_reg));
    mmu.set(aligned_addr, static_cast<Type>(value));
  }

  if constexpr (!preindex) {
    if ((load && base_reg != src_or_dest_reg) || !load) {
      const u32 writeback_addr =
          calculate_addr<add_offset_to_base>(base_value, offset);
      run_write_back(cpu, writeback_addr, instruction);
    }
  }

  return mmu.wait_cycles(addr, load_store_cycles(rd(instruction), load));
}

template <bool byte_swap>
u32 make_single_data_swap(Cpu& cpu, u32 instruction) {
  using T = std::conditional_t<byte_swap, u8, u32>;
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

template <bool immediate_offset,
          bool preindex,
          bool add_offset_to_base,
          bool word_transfer,
          bool write_back,
          bool load>
u32 make_single_data_transfer(Cpu& cpu, u32 instruction) {
  Mmu& mmu = *cpu.mmu();
  const auto calculate_offset = [&]() -> u32 {
    if constexpr (immediate_offset) {
      return instruction & 0xfff;
    }

    const ShiftResult shift_result = compute_shift_value(instruction, cpu);
    const auto [set_carry, offset] = compute_shifted_operand(shift_result);

    return offset;
  };
  const auto dest = rd(instruction);

  const u32 original_dest_value = cpu.reg(dest);

  const Register base_register = rn(instruction);
  const u32 base_value = cpu.reg(base_register);

  const u32 offset = calculate_offset();

  // The address to be used in the transfer
  const auto [aligned_addr, raw_addr,
              rotate_amount] = [&]() -> std::tuple<u32, u32, u32> {
    const u32 raw_address =
        select_addr<preindex, add_offset_to_base, write_back>(

            cpu, base_value, offset, instruction);
    return {raw_address & ~0b11, raw_address, (raw_address & 0b11) * 8};
  }();

  if constexpr (!preindex) {
    const u32 writeback_addr =
        calculate_addr<add_offset_to_base>(base_value, offset);
    run_write_back(cpu, writeback_addr, instruction);
  }

  // const bool word_transfer = word();

  if constexpr (load) {
    if constexpr (word_transfer) {
      // load a word
      const u32 loaded_value =
          rotate_right(mmu.at<u32>(aligned_addr & ~0b11), rotate_amount);
      cpu.set_reg(dest, loaded_value);
    } else {
      // load a byte
      const u8 loaded_value = mmu.at<u8>(raw_addr);
      cpu.set_reg(dest, loaded_value);
    }
  } else {
    const u32 stored_value =
        // Unspecified behavior for writeback store with base reg == dest reg
        ((write_back || !preindex) && base_register == dest
             ? original_dest_value
             : cpu.reg(dest)) +
        (dest == Register::R15 ? 4 : 0);
    if constexpr (word_transfer) {
      // Store a word
      mmu.set(aligned_addr, stored_value);
    } else {
      // Store a byte
      mmu.set(raw_addr, static_cast<u8>(stored_value & 0xff));
    }
  }
  const auto cycles = [&] {
    // Store
    if constexpr (!load) {
      return 2_nonseq;
    }

    // load
    return dest == Register::R15 ? (2_seq + 2_nonseq + 1_intern)
                                 : (1_seq + 1_nonseq + 1_intern);
  }();

  return mmu.wait_cycles(aligned_addr, cycles);
}

template <bool immediate_operand, bool use_spsr_dest, bool to_status>
u32 make_status_transfer(Cpu& cpu, u32 instruction) {
  if constexpr (to_status) {
    u32 mask = 0;

    mask |= test_bit(instruction, 19) ? 0xff000000 : 0;
    mask |= test_bit(instruction, 18) ? 0x00ff0000 : 0;
    mask |= test_bit(instruction, 17) ? 0x0000ff00 : 0;
    mask |= test_bit(instruction, 16) ? 0x000000ff : 0;
    const auto [_, operand2] =
        compute_operand2<immediate_operand>(cpu, instruction);
    const ProgramStatus next_program_status{operand2 & mask};
    if constexpr (use_spsr_dest) {
      cpu.set_saved_program_status(next_program_status);
    } else {
      cpu.set_program_status(next_program_status);
    }
  } else {
    cpu.set_reg(rd(instruction), use_spsr_dest
                                     ? cpu.saved_program_status().data()
                                     : cpu.program_status().data());
  }
  return 1;
}

template <bool add_offset_to_base>
constexpr std::tuple<std::array<Register, 16>, int> register_list(
    u32 instruction) {
  std::array<Register, 16> registers{};
  int end = 0;

  if constexpr (!add_offset_to_base) {
    for (int i = 15; i >= 0; --i) {
      if (test_bit(instruction, i)) {
        registers[end++] = static_cast<Register>(i);
      }
    }
  } else {
    for (int i = 0; i < 16; ++i) {
      if (test_bit(instruction, i)) {
        registers[end++] = static_cast<Register>(i);
      }
    }
  }
  return {registers, end};
}

template <bool preindex,
          bool add_offset_to_base,
          bool load_psr_and_user_mode,
          bool write_back,
          bool load>
u32 make_block_data_transfer(Cpu& cpu, u32 instruction) {
  const Mode current_mode = cpu.program_status().mode();
  if (load_psr_and_user_mode) {
    cpu.change_mode(Mode::User);
  }

  const auto operand_reg = rn(instruction);
  u32 offset = cpu.reg(operand_reg);

  constexpr auto change_offset = add_offset_to_base
                                     ? [](u32 val) { return val + 4; }
                                     : [](u32 val) { return val - 4; };
  const auto [registers, registers_end] =
      register_list<add_offset_to_base>(instruction);
  const nonstd::span<const Register> registers_span{registers.data(),
                                                    registers_end};

  u32 addr_cycles = 0;
  const u32 absolute_offset = 4 * registers_end;
  const u32 final_offset = add_offset_to_base ? (offset + absolute_offset)
                                              : (offset - absolute_offset);

  const bool regs_has_base =
      std::find(registers_span.begin(), registers_span.end(), operand_reg) !=
      registers_span.end();

  constexpr bool load_register = load;
  constexpr bool write_back_base = write_back;
  constexpr bool preindex_base = preindex;
  constexpr bool add_offset = add_offset_to_base;

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

  if (load_psr_and_user_mode) {
    cpu.change_mode(current_mode);
  }

  return addr_cycles + (1_nonseq + 1_intern).sum();
}

u32 branch_and_exchange(Cpu& cpu, u32 instruction) {
  common::branch_and_exchange(cpu, static_cast<Register>(instruction & 0xf));
  return 3;
}

u32 software_interrupt(Cpu& cpu, u32 instruction) {
  return execute_software_interrupt(cpu, instruction);
}

[[noreturn]] u32 invalid_instruction([[maybe_unused]] Cpu& cpu,
                                     [[maybe_unused]] u32 instruction) {
  throw std::runtime_error("invalid instruction");
}
}  // namespace arm

// using ThumbInstFunc = u32 (*)(Cpu&, u16);
static constexpr std::array<InstFunc, 4096> arm_lookup_table = [] {
  using namespace arm;
  std::array<InstFunc, 4096> res{};

  const auto decode_arm_instruction = [&res](auto i) {
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
              res[i] = make_multiply_long<test_bit(upper_bits, 2),
                                          test_bit(upper_bits, 1),
                                          test_bit(upper_bits, 0)>;
            } else {
              res[i] = make_multiply<test_bit(upper_bits, 1),
                                     test_bit(upper_bits, 0)>;
            }
            break;
          case 0b1011:
          case 0b1101:
          case 0b1111: {
            constexpr auto transfer_type =
                static_cast<TransferType>((lower_bits >> 1) & 0b11);
            if constexpr (transfer_type != TransferType::Swp) {
              res[i] = make_halfword_data_transfer<
                  false, test_bit(upper_bits, 3), test_bit(upper_bits, 2),
                  test_bit(upper_bits, 1), test_bit(upper_bits, 0),
                  transfer_type>;
            } else {
              res[i] = arm::invalid_instruction;
            }

          }
          // HalfwordDataTransfer
          break;
          default:
            res[i] = make_data_processing<false,
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
          case 0b1111: {
            // HalfwordDataTransfer
            constexpr auto transfer_type =
                static_cast<TransferType>((lower_bits >> 1) & 0b11);
            if constexpr (transfer_type != TransferType::Swp) {
              res[i] = make_halfword_data_transfer<
                  true, test_bit(upper_bits, 3), test_bit(upper_bits, 2),
                  test_bit(upper_bits, 1), test_bit(upper_bits, 0),
                  transfer_type>;
            } else {
              res[i] = arm::invalid_instruction;
            }
            break;
          }
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
        res[i] = make_block_data_transfer<preindex, add_offset_to_base,
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
  };

  // Workaround for MSVC "parser stack overflow" on for_static
  for_static<8>([&](auto base_index) {
    for_static<512>([&](auto i) {
      constexpr auto real_index = (512 * decltype(base_index)::value) + i;
      decode_arm_instruction(std::integral_constant<std::size_t, real_index>{});
    });
  });

  return res;
}();

u32 Cpu::execute() {
#if 1
  if (/*interrupts_waiting.data() > 0 ||*/ halted) {
    return 1;
  }
#endif

  if (const u32 pc_region = m_regs[15] & 0xff000000;
      m_current_memory_region != pc_region) {
    const auto [storage, _] = m_mmu->select_storage(reg(Register::R15));
    m_current_memory_region = pc_region;
    m_current_memory = storage;
    m_memory_offset = pc_region;
  }

  if (m_current_program_status.thumb_mode()) {
    const u32 pc = (m_regs[15] & ~0b1);
    // fmt::printf("%08x\n", pc);

    u16 instruction;
    std::memcpy(&instruction, &m_current_memory[pc - m_memory_offset],
                sizeof(u16));
    m_regs[15] = pc + 2;

    if (const u32 condition = (instruction >> 8) & 0b1111;
        (instruction & 0b1101'0000'0000'0000) == 0b1101'0000'0000'0000 &&
        ((instruction >> 12) != 0b1111) && condition != 0b1111) {
      if (should_execute(condition << 28, m_current_program_status)) {
        return thumb::conditional_branch(*this, instruction);
      }
      return 0;
    }
    const auto inst_func =
        thumb_instruction_tables.interpreter_table[(instruction >> 6) & 0x3ff];
    return inst_func(*this, instruction);
  }

  const u32 pc = (m_regs[15] & ~0b11);
  // fmt::printf("%08x\n", pc);
  u32 arm_instruction;
  std::memcpy(&arm_instruction, &m_current_memory[pc - m_memory_offset],
              sizeof(u32));
  m_regs[15] = pc + 4;
  const auto inst_func =
      arm_lookup_table[(((arm_instruction >> 20) & 0xff) << 4) |
                       ((arm_instruction >> 4) & 0b1111)];

  if (should_execute(arm_instruction, m_current_program_status)) {
    return inst_func(*this, arm_instruction);
  }
  return 0;
}

}  // namespace gb::advance
