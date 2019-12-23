#include "gba/interrupts.h"
#include "gba/cpu.h"

namespace gb::advance {
void InterruptsRequested::set_interrupt(Interrupt interrupt, bool set) {
  InterruptBucket::set_interrupt(interrupt, set);

  if (const auto data = m_cpu->interrupts_waiting.data();
      (m_cpu->interrupts_requested.data() & data) != 0) {
    m_cpu->interrupts_waiting.set_data(0);
  }
  if ((m_cpu->interrupts_enabled.data() & m_cpu->interrupts_requested.data()) !=
      0) {
    m_cpu->halted = false;
    if (gb::test_bit(m_cpu->ime, 0) && m_cpu->program_status().irq_enabled()) {
      m_cpu->handle_interrupts();
    }
  }
}
}  // namespace gb::advance
