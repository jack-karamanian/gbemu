#include "gba/dma.h"
#include "gba/cpu.h"
#include "gba/mmu.h"

namespace gb::advance {

static Mmu::AddrOp select_addr_op(Dma::Control::AddrControl control) {
  switch (control) {
    case Dma::Control::AddrControl::Increment:
    case Dma::Control::AddrControl::IncrementAndReload:
      return Mmu::AddrOp::Increment;
    case Dma::Control::AddrControl::Decrement:
      return Mmu::AddrOp::Decrement;
    case Dma::Control::AddrControl::Fixed:
      return Mmu::AddrOp::Fixed;
  }
  GB_UNREACHABLE();
}

void Dma::run() {
  m_control.set_data(m_control.data() & ~0b1'1111);

  const auto source_op = select_addr_op(m_control.source_addr_control());
  const auto dest_op = select_addr_op(m_control.dest_addr_control());

  const u32 type_size = m_control.word_transfer() ? 4 : 2;

  constexpr u32 byte_mask = 0x0ffffffe;

  const u32 masked_source = (m_internal_source & m_source_mask) & byte_mask;
  const u32 masked_dest = (m_internal_dest & m_dest_mask) & byte_mask;

  const u32 final_count =
      count != 0 ? count : m_dma_number == DmaNumber::Dma3 ? 0x10000 : 0x4000;

  m_mmu->copy_memory({masked_source, source_op}, {masked_dest, dest_op},
                     final_count, type_size);

  // fmt::printf("type size %d count %d m_internal_source %08x\n", type_size,
  //            m_internal_count, m_internal_source);
  if (m_control.dest_addr_control() !=
      Control::AddrControl::IncrementAndReload) {
    m_internal_dest += final_count * (type_size * static_cast<int>(dest_op));
  }
  m_internal_source += final_count * (type_size * static_cast<int>(source_op));

  if (!m_control.repeat()) {
    m_control.set_enabled(false);
  }
  if (m_control.interrupt_at_end()) {
    m_cpu->interrupts_requested.set_interrupt(m_dma_interrupt, true);
  }
}

}  // namespace gb::advance
