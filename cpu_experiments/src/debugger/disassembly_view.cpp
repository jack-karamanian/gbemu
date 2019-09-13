
#include "disassembly_view.h"
#include <cstdlib>
#include "gba/cpu.h"
#include "imgui.h"

namespace gb::advance {
void DisassemblyView::render(gb::u32 base,
                             const Cpu& cpu,
                             const DisassemblyInfo& disassembly_info) {
  static std::array<char, 9> m_number_buffer;
  ImGui::Begin(m_name);

  std::optional<u32> target_addr;

  const auto go_to_address = [&](u32 address) { target_addr = address; };

  ImGui::InputTextWithHint("", "Address", m_number_buffer.data(), 9);
  ImGui::SameLine();
  if (ImGui::Button("Goto")) {
    char* end = nullptr;
    const u32 addr = std::strtol(m_number_buffer.begin(), &end, 16);
    go_to_address(addr - base);
  }

  ImGui::BeginChild("Inner");
  ImGuiListClipper clipper(disassembly_info.disassembly.size(),
                           ImGui::GetTextLineHeightWithSpacing());
  const gb::u32 offset =
      (cpu.reg(gb::advance::Register::R15) - cpu.prefetch_offset()) - base;

  if (offset != m_prev_offset) {
    go_to_address(offset);
    m_prev_offset = offset;
  }

  if (target_addr) {
    const auto index = disassembly_info.addr_to_index.find(*target_addr);
    if (index != disassembly_info.addr_to_index.end()) {
      const float adjusted_offset =
          ImGui::GetCursorStartPos().y +
          index->second * (ImGui::GetTextLineHeightWithSpacing());
      ImGui::SetScrollFromPosY(adjusted_offset);
    }
    target_addr = {};
  }

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      const auto& entry = disassembly_info.disassembly[i];

      if (entry.loc == offset) {
        ImGui::Text("-> %08x %s", entry.loc + base, entry.text.c_str());
      } else {
        ImGui::Text("%08x %s", entry.loc + base, entry.text.c_str());
      }
    }
  }
  ImGui::EndChild();
  ImGui::End();
}
}  // namespace gb::advance
