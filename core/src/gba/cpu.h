#pragma once
#include <doctest/doctest.h>
#include <array>
#include <functional>
#include <nonstd/span.hpp>
#include "error_handling.h"
#include "interrupts.h"
#include "mmu.h"
#include "types.h"
#include "utils.h"

namespace gb::advance {

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

class Cpu {
 public:
  Cpu() = default;
  Cpu(Mmu& mmu) : m_mmu{&mmu} {}

  [[nodiscard]] constexpr u32 prefetch_offset() const {
    return m_current_program_status.thumb_mode() ? 2 : 4;
  }

  [[nodiscard]] constexpr u32 reg(Register reg_selected) const {
    const u32 index = static_cast<u32>(reg_selected);
    assert(index < m_regs.size());

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
    const auto mode = m_current_program_status.mode();
    if (mode == Mode::User || mode == Mode::System) {
      return m_current_program_status;
    }
    return m_saved_program_status[index_from_mode(
        m_current_program_status.mode())];
  }

  constexpr void set_saved_program_status(ProgramStatus program_status) {
    const auto mode = m_current_program_status.mode();
    if (mode != Mode::User && mode != Mode::System) {
      m_saved_program_status[index_from_mode(m_current_program_status.mode())] =
          program_status;
    }
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

  void soft_reset() {
    change_mode(Mode::System);
    for (u32 i = 0; i < 13; ++i) {
      set_reg(static_cast<Register>(i), 0);
    }
    // R14 = 0
    m_saved_registers.irq[1] = 0;
    m_saved_registers.irq[0] = 0x03007fa0;

    set_reg(Register::R13, 0x03007f00);

    const auto iwram = m_mmu->iwram();

    const u8 jump_flag = m_mmu->at<u8>(0x03007ffa);

    std::fill(iwram.begin() + 0x7e00, iwram.end(), 0);

    m_saved_program_status[index_from_mode(Mode::IRQ)] = ProgramStatus{0};

    set_thumb(false);

    set_reg(Register::R15, jump_flag == 0 ? 0x08000000 : 0x02000000);
  }

  struct Debugger {
    std::function<void()> stop_execution;
  };

  void set_debugger(Debugger debugger) { m_debugger = std::move(debugger); }

  [[nodiscard]] const Debugger& debugger() const { return m_debugger; }

  InterruptBucket interrupts_enabled{0};
  InterruptsRequested interrupts_requested{};
  InterruptBucket interrupts_waiting{0};
  u32 ime = 0;

  template <typename T>
  constexpr u32 run_instruction(T instruction) {
    if (instruction.should_execute(program_status())) {
      return instruction.execute(*this);
    }
    return 0;
  }

  [[nodiscard]] u32 execute();
  void handle_interrupts();

  [[nodiscard]] nonstd::span<const u8> prefetched_opcode() const noexcept {
    return m_prefetched_opcode;
  }

  bool halted = false;

  [[nodiscard]] Mmu* mmu() const noexcept { return m_mmu; }

 private:
  struct SavedRegisters {
    // R8-R14
    std::array<u32, 7> system_and_user{{0, 0, 0, 0, 0, 0, 0}};
    std::array<u32, 7> fiq{{0, 0, 0, 0, 0, 0, 0}};
    // R13-R14
    std::array<u32, 2> supervisor{{0x03007FE0, 0}};
    std::array<u32, 2> abort{{0, 0}};
    std::array<u32, 2> irq{{0, 0}};
    std::array<u32, 2> undefined{{0, 0}};
  };

  constexpr ProgramStatus& get_current_program_status() {
    return m_current_program_status;
  }

  [[nodiscard]] static constexpr u32 index_from_mode(Mode mode) {
    return static_cast<u32>(mode) - 1;
  }
  std::array<u32, 16> m_regs = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  Mmu* m_mmu = nullptr;
  Debugger m_debugger;
  ProgramStatus m_current_program_status{};
  std::array<ProgramStatus, 5> m_saved_program_status{};
  SavedRegisters m_saved_registers;
  u32 m_prefetch_offset = 4;

  nonstd::span<const u8> m_current_memory;
  u32 m_current_memory_region = 0;
  u32 m_memory_offset = 0;
  std::array<u8, 4> m_prefetched_opcode = {0, 0, 0, 0};
};

u32 execute_software_interrupt(Cpu& cpu, u32 instruction);

}  // namespace gb::advance
