#include "disassembly_view.h"
#include "gba/cpu.h"
#include "imgui.h"

namespace gb::advance {
void DisassemblyView::render(gb::u32 base,
                             const Cpu& cpu,
                             const DisassemblyInfo& disassembly_info) {
  ImGui::Begin(m_name);
  ImGuiListClipper clipper(disassembly_info.disassembly.size(),
                           ImGui::GetTextLineHeightWithSpacing());
  const gb::u32 offset =
      (cpu.reg(gb::advance::Register::R15) - cpu.prefetch_offset()) - base;

  if (offset != m_prev_offset) {
    const auto index = disassembly_info.addr_to_index.find(offset);
    if (index != disassembly_info.addr_to_index.end()) {
      const float adjusted_offset =
          ImGui::GetCursorStartPos().y +
          index->second * (ImGui::GetTextLineHeightWithSpacing());
      ImGui::SetScrollFromPosY(adjusted_offset);
    }
    m_prev_offset = offset;
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
  ImGui::End();
}
}  // namespace gb::advance
