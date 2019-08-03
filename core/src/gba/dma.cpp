#include <doctest/doctest.h>
#include "dma.h"
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

static Interrupt dma_number_to_interrupt(Dma::DmaNumber dma_number) {
  switch (dma_number) {
    case Dma::DmaNumber::Dma0:
      return Interrupt::Dma0;
    case Dma::DmaNumber::Dma1:
      return Interrupt::Dma1;
    case Dma::DmaNumber::Dma2:
      return Interrupt::Dma2;
    case Dma::DmaNumber::Dma3:
      return Interrupt::Dma3;
  }
}

void Dma::run() {
  fmt::print("RUN DMA\n");
  const u32 masked_source = source & m_source_mask;
  const u32 masked_dest = m_internal_dest & m_dest_mask;
  fmt::printf("source %08x\n dest %08x\n", masked_source, masked_dest);

  const auto source_op = select_addr_op(m_control.source_addr_control());
  const auto dest_op = select_addr_op(m_control.dest_addr_control());

  const u32 type_size = m_control.word_transfer() ? 4 : 2;
  m_mmu->copy_memory({masked_source, source_op}, {masked_dest, dest_op}, count,
                     type_size);

  if (m_control.dest_addr_control() !=
      Control::AddrControl::IncrementAndReload) {
    const u32 transfer_size =
        m_control.word_transfer() ? sizeof(u32) : sizeof(u16);
    m_internal_dest = count * transfer_size;
  }

  if (!m_control.repeat() &&
      m_control.start_timing() != Control::StartTiming::Immediately) {
    m_control.set_enabled(false);
  }
  if (m_control.interrupt_at_end()) {
    m_cpu->interrupts_requested.set_interrupt(
        dma_number_to_interrupt(m_dma_number), true);
  }
}

template <typename T>
struct ExtractDmaNumber {};

template <Dma::DmaNumber Num>
struct ExtractDmaNumber<Dma::Addresses<Num>> {
  static constexpr Dma::DmaNumber value = Num;
};

std::tuple<Dma::AddressType, Dma::DmaNumber, bool> Dma::select_dma(u32 addr) {
  using DmaNumbers =
      std::tuple<Addresses<DmaNumber::Dma0>, Addresses<DmaNumber::Dma1>,
                 Addresses<DmaNumber::Dma2>, Addresses<DmaNumber::Dma3>>;
  std::tuple<AddressType, DmaNumber, bool> found{AddressType::Source,
                                                 DmaNumber::Dma0, false};
  for_static<std::tuple_size_v<DmaNumbers>>([addr, &found](auto i) {
    using T = std::tuple_element_t<i, DmaNumbers>;
    constexpr DmaNumber dma_number = ExtractDmaNumber<T>::value;
    switch (addr) {
      case T::Source:
        found = {AddressType::Source, dma_number, true};
        break;
      case T::Dest:
        found = {AddressType::Dest, dma_number, true};
        break;
      case T::Count:
        found = {AddressType::Count, dma_number, true};
        break;
      case T::Control:
        found = {AddressType::Control, dma_number, true};
        break;
    }
  });
  return found;
}

TEST_CASE("dma address lookups should return the correct dma") {
  const auto [address_type, dma_number, found] =
      Dma::select_dma(Dma::Addresses<Dma::DmaNumber::Dma3>::Control);
  CHECK(address_type == Dma::AddressType::Control);
  CHECK(dma_number == Dma::DmaNumber::Dma3);
  CHECK(found == true);
}
}  // namespace gb::advance
