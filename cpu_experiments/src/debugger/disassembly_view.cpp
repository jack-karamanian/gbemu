
#include "disassembly_view.h"
#include "gba/cpu.h"
#include "imgui.h"

namespace gb::advance {
void DisassemblyView::render(u32 base,
                             int instr_size,
                             const Cpu& cpu,
                             DisassemblyInfo& disassembly_info) {
  if (instr_size != m_prev_instr_size) {
    m_prev_instr_size = instr_size;
    auto begin = disassembly_info.disassembly_cache.begin();
    auto end = disassembly_info.disassembly_cache.end();
    std::fill(begin, end, std::string{});
  }

  ImGui::Begin(m_name);

  std::optional<u32> target_addr;

  const auto go_to_address = [&target_addr](u32 address) {
    target_addr = address;
  };

  ImGui::InputTextWithHint("", "Address", m_number_buffer.data(),
                           m_number_buffer.size());
  ImGui::SameLine();
  if (ImGui::Button("Goto")) {
    char* end = nullptr;
    const u32 addr = std::strtol(m_number_buffer.begin(), &end, 16);
    go_to_address(addr - base);
  }

  ImGui::BeginChild("Inner");
  const u32 current_pc = (cpu.reg(gb::advance::Register::R15) - instr_size);
  const auto storage = cpu.mmu()->select_storage(current_pc).storage;
  if (m_prev_size != storage.size()) {
    m_prev_size = storage.size();
    m_num_potential_instructions = storage.size() / instr_size;
    disassembly_info.disassembly_cache.resize(m_num_potential_instructions);
  }

  ImGuiListClipper clipper(m_num_potential_instructions,
                           ImGui::GetTextLineHeightWithSpacing());
  const gb::u32 offset = current_pc - base;

  if (offset != m_prev_offset) {
    go_to_address(offset);
    m_prev_offset = offset;
  }

  if (target_addr) {
    const auto target_index = *target_addr / instr_size;
    const float adjusted_offset =
        ImGui::GetCursorStartPos().y +
        target_index * (ImGui::GetTextLineHeightWithSpacing());
    ImGui::SetScrollFromPosY(adjusted_offset);
  }

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      const u32 instr_index = i * instr_size;
      const auto& text = [&] {
        auto& entry = disassembly_info.disassembly_cache[i];
        if (entry.empty()) {
          auto res =
              experiments::disassemble(storage.subspan(instr_index, instr_size),
                                       instr_size == 2 ? "thumb" : "arm");
          assert(res.size() == 1);
          return disassembly_info.disassembly_cache[i] = std::move(res[0].text);
        }
        return entry;
      }();

      if (instr_index == offset) {
        ImGui::Text("-> %08x %s", instr_index + base, text.c_str());
      } else {
        ImGui::Text("%08x %s", instr_index + base, text.c_str());
      }
    }
  }
  ImGui::EndChild();
  ImGui::End();
}
}  // namespace gb::advance
