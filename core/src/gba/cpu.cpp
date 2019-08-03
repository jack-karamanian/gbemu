#include "cpu.h"

namespace gb::advance {
void SoftwareInterrupt::cpu_set(Cpu& cpu) {
  const u32 source = cpu.reg(Register::R0);
  const u32 dest = cpu.reg(Register::R1);
  const u32 control = cpu.reg(Register::R2);

  const u32 type_size = gb::test_bit(control, 26) ? 4 : 2;
  const u32 count = control & 0x1fffff;

  const bool fixed_source = gb::test_bit(control, 24);

  cpu.m_mmu->copy_memory(
      {source, fixed_source ? Mmu::AddrOp::Fixed : Mmu::AddrOp::Increment},
      {dest, Mmu::AddrOp::Increment}, count, type_size);
}

enum class SoftwareInterruptType : u32 {
  CpuSet = 0x0b,
};

u32 SoftwareInterrupt::execute(Cpu& cpu) {
  const auto interrupt_type =
      static_cast<SoftwareInterruptType>(m_value & 0x00ffffff);

  switch (interrupt_type) {
    case SoftwareInterruptType::CpuSet:
      cpu_set(cpu);
      break;
    default:
      throw std::runtime_error("unimplemented swi");
  }

  // 2S + 1N
  return 3;
}

u32 Cpu::handle_interrupts() {
  if (gb::test_bit(m_mmu->ime, 0) && m_current_program_status.irq_enabled() &&
      (interrupts_enabled.data() & interrupts_requested.data()) != 0) {
    const u32 next_pc = reg(Register::R15) + prefetch_offset() * 2;

    set_saved_program_status_for_mode(Mode::IRQ, m_current_program_status);
    change_mode(Mode::IRQ);
    set_reg(Register::R14, next_pc);

    m_current_program_status.set_irq_enabled(false);
    set_thumb(false);

    set_reg(Register::R15, 0x00000128);
    fmt::print("handling interrupts\n");
  }
  return 0;
}
}  // namespace gb::advance
