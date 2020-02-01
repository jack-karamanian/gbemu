#include "gba/assembler.h"
#include <fmt/printf.h>
#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCAsmBackend.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCCodeEmitter.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCObjectWriter.h>
#include <llvm/MC/MCParser/MCAsmParser.h>
#include <llvm/MC/MCParser/MCTargetAsmParser.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/Object/ELF.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <iostream>

namespace experiments {
static const std::string triple_name = "arm-none-eabi";
static llvm::SmallVector<char, 64> assemble_elf(const std::string& src) {
  std::string error;

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();

  llvm::Triple triple(llvm::Triple::normalize(triple_name));

  const llvm::Target* target =
      llvm::TargetRegistry::lookupTarget("arm", triple, error);
  if (!target) {
    fmt::printf("Error %s\n", error.c_str());
  }

  llvm::SourceMgr src_mgr;
  {
    auto src_buffer = llvm::MemoryBuffer::getMemBuffer(src);
    src_mgr.AddNewSourceBuffer(std::move(src_buffer), llvm::SMLoc{});
  }

  std::unique_ptr<llvm::MCRegisterInfo> register_info{
      target->createMCRegInfo(triple_name)};

  std::unique_ptr<llvm::MCAsmInfo> asm_info{
      target->createMCAsmInfo(*register_info, triple_name)};

  llvm::MCObjectFileInfo object_file_info;
  llvm::MCContext mc_context{asm_info.get(), register_info.get(),
                             &object_file_info, &src_mgr};

  object_file_info.InitMCObjectFileInfo(triple, true, mc_context);

  std::unique_ptr<llvm::MCInstrInfo> instr_info{target->createMCInstrInfo()};

  std::unique_ptr<llvm::MCSubtargetInfo> subtarget_info{
      target->createMCSubtargetInfo(triple.getTriple(), "arm7tdmi", "armv4t")};

  llvm::SmallVector<char, 64> elf_bytes;
  llvm::raw_svector_ostream ostream{elf_bytes};

  std::unique_ptr<llvm::MCCodeEmitter> code_emitter{
      target->createMCCodeEmitter(*instr_info, *register_info, mc_context)};

  llvm::MCAsmBackend* asm_backend = target->createMCAsmBackend(
      *subtarget_info, *register_info, llvm::MCTargetOptions{});

  std::unique_ptr<llvm::MCStreamer> stream{target->createMCObjectStreamer(
      triple, mc_context, std::unique_ptr<llvm::MCAsmBackend>{asm_backend},
      asm_backend->createObjectWriter(ostream), std::move(code_emitter),
      *subtarget_info, false, false, false)};
  std::unique_ptr<llvm::MCAsmParser> asm_parser{
      llvm::createMCAsmParser(src_mgr, mc_context, *stream, *asm_info)};

  std::unique_ptr<llvm::MCTargetAsmParser> target_asm_parser{
      target->createMCAsmParser(*subtarget_info, *asm_parser, *instr_info,
                                llvm::MCTargetOptions{})};
  asm_parser->setTargetParser(*target_asm_parser);

  asm_parser->Run(false);

  return elf_bytes;
}

std::vector<gb::u8> assemble(const std::string& src) {
  auto elf_bytes = assemble_elf(src);

  auto elf = llvm::object::ELF32LEFile::create(
      llvm::StringRef(elf_bytes.data(), elf_bytes.size()));
  if (!elf) {
    std::cerr << "Failed to parse elf\n";
    return {};
  }
  auto section = elf->getSection(".text");

  auto contents = elf->getSectionContents(section.get());

  return std::vector<gb::u8>(contents->begin(), contents->end());
}

std::vector<DisassemblyEntry> disassemble(nonstd::span<gb::u8> bytes,
                                          std::string_view arch) {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllDisassemblers();

  llvm::SmallString<256> asm_string;
  llvm::raw_svector_ostream ostream{asm_string};
  std::string error;

  llvm::Triple triple(llvm::Triple::normalize(triple_name));

  const llvm::Target* target =
      llvm::TargetRegistry::lookupTarget(arch.data(), triple, error);

  if (target == nullptr) {
    fmt::printf("Error %s\n", error.c_str());
    return {};
  }

  llvm::SourceMgr src_mgr;

  std::unique_ptr<llvm::MCRegisterInfo> register_info{
      target->createMCRegInfo(triple.getTriple())};

  std::unique_ptr<llvm::MCAsmInfo> asm_info{
      target->createMCAsmInfo(*register_info, triple.getTriple())};

  llvm::MCObjectFileInfo object_file_info;

  llvm::MCContext mc_context{asm_info.get(), register_info.get(), nullptr};

  std::unique_ptr<llvm::MCInstrInfo> instr_info{target->createMCInstrInfo()};

  llvm::MCInstPrinter* inst_printer =
      target->createMCInstPrinter(triple, asm_info->getAssemblerDialect(),
                                  *asm_info, *instr_info, *register_info);

  std::unique_ptr<llvm::MCCodeEmitter> code_emitter{
      target->createMCCodeEmitter(*instr_info, *register_info, mc_context)};

  std::unique_ptr<llvm::MCSubtargetInfo> subtarget_info{
      target->createMCSubtargetInfo(triple.getTriple(), "arm7tdmi", "armv4t")};

  llvm::MCAsmBackend* asm_backend = target->createMCAsmBackend(
      *subtarget_info, *register_info, llvm::MCTargetOptions{});

  std::unique_ptr<llvm::MCStreamer> stream{target->createAsmStreamer(
      mc_context, std::make_unique<llvm::formatted_raw_ostream>(ostream), true,
      false, inst_printer, std::move(code_emitter),
      std::unique_ptr<llvm::MCAsmBackend>{asm_backend}, false)};

  std::unique_ptr<llvm::MCDisassembler> disassembler{
      target->createMCDisassembler(*subtarget_info, mc_context)};

  uint64_t size = 0;

  llvm::ArrayRef<gb::u8> bytes_ref{bytes.data(),
                                   static_cast<std::size_t>(bytes.size())};

  llvm::SmallString<512> line_string;
  llvm::raw_svector_ostream line_stream{line_string};

  std::vector<DisassemblyEntry> asm_lines;

  const gb::u32 instruction_size = [arch] {
    if (arch == "arm") {
      return 4;
    }
    if (arch == "thumb") {
      return 2;
    }

    return 1;
  }();

  for (uint64_t i = 0; i < bytes_ref.size(); i += size) {
    llvm::MCInst inst;
    auto status = disassembler->getInstruction(inst, size, bytes_ref.slice(i),
                                               i, llvm::nulls(), llvm::nulls());
    if (instruction_size == 2 && status == llvm::MCDisassembler::Fail &&
        bytes_ref.size() == 2) {
      // Try to disassemble a BL instruction
      llvm::ArrayRef<gb::u8> bl_ref{bytes.data(), 4};

      status = disassembler->getInstruction(inst, size, bl_ref.slice(i), i,
                                            llvm::nulls(), llvm::nulls());
    }

    switch (status) {
      case llvm::MCDisassembler::Fail:
        if (size == 0) {
          size = instruction_size;
        }
        asm_lines.emplace_back("invalid", i);
        break;
      case llvm::MCDisassembler::SoftFail:
      case llvm::MCDisassembler::Success:

        // stream->AddComment(std::string{"Offset: "} + std::to_string(i));
        stream->EmitInstruction(inst, *subtarget_info);
        asm_lines.emplace_back(
            std::string(asm_string.begin(), asm_string.end()), i);
        asm_string.clear();
        break;
    }
  }

  return asm_lines;
}
}  // namespace experiments
