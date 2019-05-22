#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCAsmBackend.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCCodeEmitter.h>
#include <llvm/MC/MCContext.h>
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
#include "assembler.h"

namespace experiments {
static llvm::SmallVector<char, 64> assemble_elf(const std::string& src) {
  const std::string triple_name = "arm-none-eabi";
  std::string error;

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();

  llvm::Triple triple(llvm::Triple::normalize(triple_name));

  const llvm::Target* target =
      llvm::TargetRegistry::lookupTarget("arm", triple, error);
  if (!target) {
    printf("Error %s\n", error.c_str());
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
      target->createMCSubtargetInfo(triple_name, "arm7tdmi", "armv4t")};

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
  if (!asm_parser) {
    printf("null\n");
  }

  asm_parser->Run(false);

  return elf_bytes;
}

std::vector<gb::u8> assemble(const std::string& src) {
  auto elf_bytes = assemble_elf(src);
  for (char c : elf_bytes) {
    std::cout << c;
  }
  std::cout << '\n';

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
}  // namespace experiments
