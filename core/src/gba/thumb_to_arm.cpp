#include "assembler.h"
#include "cpu.h"
#include "thumb_to_arm.h"

namespace gb::advance {
enum class ThumbInstructionType {
  MoveShiftedRegister = 0,
  AddSubtract,
  MoveCompareAddSubtractImm,
  AluOperation,
  HiRegisterOperation,
  PcRelativeLoad,
  LoadStoreRegisterOffset,
  LoadStoreSignExtendedByteHalfword,
  LoadStoreImmOffset,
  LoadStoreHalfword,
  SpRelativeLoadStore,
  LoadAddress,
  AddOffsetToStackPointer,
  PushPopRegisters,
  MultipleLoadStore,
  ConditionalBranch,
  SoftwareInterrupt,
  UnconditionalBranch,
  LongBranchLink,
  Undefined = 45545,
};

using ThumbLookupEntry = InstructionLookupEntry<u16, ThumbInstructionType>;

[[nodiscard]] static constexpr u32 make_data_processing(
    Opcode opcode,
    bool immediate_operand,
    bool set_condition_code) {
  return 0b1110'00'0'0000'0'0000'0000'000000000000 |
         (immediate_operand ? 1 << 25 : 0) | (static_cast<u32>(opcode) << 21) |
         (set_condition_code ? 1 << 20 : 0);
}

[[nodiscard]] static u32 with_registers(u32 instruction,
                                        u32 dest,
                                        u32 op1,
                                        u32 op2) {
  return instruction | (op1 << 16) | (dest << 12) | op2;
}

constexpr std::array<ThumbLookupEntry, 19> generate_thumb_lookup_table() {
  std::array<ThumbLookupEntry, 19> res = {{
      ThumbLookupEntry{ThumbInstructionType::MoveShiftedRegister}.mask_bits(
          15, 14, 13),
      ThumbLookupEntry{ThumbInstructionType::AddSubtract}
          .mask_bit_range(11, 15)
          .expect_bits(12, 11),
      ThumbLookupEntry{ThumbInstructionType::MoveCompareAddSubtractImm}
          .mask_bit_range(13, 15)
          .expect_bits(13),
      ThumbLookupEntry{ThumbInstructionType::AluOperation}
          .mask_bit_range(10, 15)
          .expect_bits(14),
      ThumbLookupEntry{ThumbInstructionType::HiRegisterOperation}
          .mask_bit_range(10, 15)
          .expect_bits(10, 14),
      ThumbLookupEntry{ThumbInstructionType::PcRelativeLoad}
          .mask_bit_range(11, 15)
          .expect_bits(11, 14),
      ThumbLookupEntry{ThumbInstructionType::LoadStoreRegisterOffset}
          .mask_bit_range(12, 15)
          .mask_bits(9)
          .expect_bits(12, 14),
      ThumbLookupEntry{ThumbInstructionType::LoadStoreSignExtendedByteHalfword}
          .mask_bit_range(12, 15)
          .mask_bits(9)
          .expect_bits(9, 12, 14),
      ThumbLookupEntry{ThumbInstructionType::LoadStoreImmOffset}
          .mask_bit_range(13, 15)
          .expect_bits(13, 14),
      ThumbLookupEntry{ThumbInstructionType::LoadStoreHalfword}
          .mask_bit_range(12, 15)
          .expect_bits(15),
      ThumbLookupEntry{ThumbInstructionType::SpRelativeLoadStore}
          .mask_bit_range(12, 15)
          .expect_bits(12, 15),
      ThumbLookupEntry{ThumbInstructionType::LoadAddress}
          .mask_bit_range(12, 15)
          .expect_bits(13, 15),
      ThumbLookupEntry{ThumbInstructionType::AddOffsetToStackPointer}
          .mask_bit_range(8, 15)
          .expect_bits(12, 13, 15),
      ThumbLookupEntry{ThumbInstructionType::PushPopRegisters}
          .mask_bit_range(12, 15)
          .mask_bits(10, 9)
          .expect_bits(10, 12, 13, 15),
      ThumbLookupEntry{ThumbInstructionType::MultipleLoadStore}
          .mask_bit_range(12, 15)
          .expect_bits(13, 15),
      ThumbLookupEntry{ThumbInstructionType::ConditionalBranch}
          .mask_bit_range(12, 15)
          .expect_bits(12, 14, 15),
      ThumbLookupEntry{ThumbInstructionType::SoftwareInterrupt}
          .mask_bit_range(8, 15)
          .expect_bits(8, 9, 10, 11, 12, 14, 15),
      ThumbLookupEntry{ThumbInstructionType::UnconditionalBranch}
          .mask_bit_range(11, 15)
          .expect_bits(13, 14, 15),
      ThumbLookupEntry{ThumbInstructionType::LongBranchLink}
          .mask_bit_range(11, 15)
          .expect_bits(12, 13, 14, 15),
  }};

  constexpr_sort(res.begin(), res.end(),
                 [](auto a, auto b) { return b.mask < a.mask; });

  return res;
}

static constexpr InstructionTable<u16,
                                  ThumbInstructionType,
                                  generate_thumb_lookup_table>
    thumb_lookup_table;

u32 convert_thumb_to_arm(u16 instruction) {
  const ThumbInstructionType instruction_type =
      thumb_lookup_table.decode_instruction_type(instruction);

  const u8 dest_reg = instruction & 0x7;
  const u8 operand_reg = (instruction & 0x38) >> 3;

  switch (instruction_type) {
    case ThumbInstructionType::MoveShiftedRegister: {
      const u8 offset = (instruction >> 6) & 0x1f;
      const u8 opcode = (instruction >> 11) & 0b11;
      constexpr u32 base_instruction =
          make_data_processing(Opcode::Mov, false, true);
#if 0
      const u32 arm_instruction = base_instruction | (dest_reg << 12) |
                                  (offset << 7) | (opcode << 5) | operand_reg;
#endif
      const u32 arm_instruction =
          with_registers(base_instruction, dest_reg, 0, operand_reg) |
          (offset << 7) | (opcode << 5);
      return arm_instruction;
    }
    case ThumbInstructionType::AddSubtract: {
      const bool sub = test_bit(instruction, 9);
      const bool immediate_operand = test_bit(instruction, 10);
      const u32 immediate_flag = immediate_operand ? 1 << 25 : 0;
      const u32 opcode = static_cast<u32>(sub ? Opcode::Sub : Opcode::Add)
                         << 21;

      const u32 operand_2 = (instruction >> 6) & 0x7;

      constexpr u32 base_instruction =
          make_data_processing(Opcode::And, false, true);

      const u32 arm_instruction{base_instruction | immediate_flag | opcode |
                                (operand_reg << 16) | (dest_reg << 12) |
                                operand_2};
      return arm_instruction;
    }

    case ThumbInstructionType::MoveCompareAddSubtractImm: {
      const u32 dest_reg_override = (instruction >> 8) & 0x7;
      const u32 offset = instruction & 0xff;

      const Opcode opcode = [instruction] {
        switch ((instruction >> 11) & 0b11) {
          case 0:
            return Opcode::Mov;
          case 1:
            return Opcode::Cmp;
          case 2:
            return Opcode::Add;
          case 3:
            return Opcode::Sub;
        }
        throw std::runtime_error(
            "invalid opcode; should not have been reached");
      }();

      constexpr u32 base_instruction =
          make_data_processing(Opcode::And, true, true);

      const u32 arm_instruction{
          base_instruction | (static_cast<u32>(opcode) << 21) |
          (opcode != Opcode::Mov ? dest_reg_override << 16 : 0) |
          (opcode != Opcode::Cmp ? dest_reg_override << 12 : 0) | offset};

      return arm_instruction;
    }
    case ThumbInstructionType::AluOperation: {
      enum class AluOperationOpcode : u32 {
        And = 0b0000,
        Eor = 0b0001,
        Lsl = 0b0010,
        Lsr = 0b0011,
        Asr = 0b0100,
        Adc = 0b0101,
        Sbc = 0b0110,
        Ror = 0b0111,
        Tst = 0b1000,
        Neg = 0b1001,
        Cmp = 0b1010,
        Cmn = 0b1011,
        Orr = 0b1100,
        Mul = 0b1101,
        Bic = 0b1110,
        Mvn = 0b1111,
      };
      static constexpr std::array<Opcode, 16> alu_opcode_to_data_processing = {
          Opcode::And, Opcode::Eor, Opcode::Mov, Opcode::Mov, Opcode::Mov,
          Opcode::Adc, Opcode::Sbc, Opcode::Mov, Opcode::Tst, Opcode::Rsb,
          Opcode::Cmp, Opcode::Cmn, Opcode::Orr,
          Opcode::And,  // Multiply. Should never be read.
          Opcode::Bic, Opcode::Mvn,
      };

      const u32 opcode_value = (instruction >> 6) & 0xf;
      const AluOperationOpcode opcode =
          static_cast<AluOperationOpcode>(opcode_value);

      const u32 data_processing_opcode =
          static_cast<u32>(alu_opcode_to_data_processing[opcode_value]);

      const u32 arm_instruction{0b1110'000'0000'1'0000'0000'000000000000 |
                                (data_processing_opcode << 21)};

      switch (opcode) {
        case AluOperationOpcode::Ror:
        case AluOperationOpcode::Lsl:
        case AluOperationOpcode::Lsr:
        case AluOperationOpcode::Asr: {
          const u32 shift_opcode = opcode != AluOperationOpcode::Ror
                                       ? static_cast<u32>(opcode) - 2
                                       : 0b11;
          // Use a MOVS instruction
          const u32 shift_arm_instruction{
              0b1110'000'1101'1'0000'0000'000000010000 | (shift_opcode << 5) |
              (operand_reg << 8) | (dest_reg << 12) | dest_reg

          };
          return shift_arm_instruction;
        }
        case AluOperationOpcode::And:
        case AluOperationOpcode::Eor:
        case AluOperationOpcode::Adc:
        case AluOperationOpcode::Sbc:
        case AluOperationOpcode::Orr:
        case AluOperationOpcode::Bic:
          return with_registers(arm_instruction, dest_reg, dest_reg,
                                operand_reg);
          // return arm_instruction | (dest_reg << 16) | (dest_reg << 12) |
          //       (operand_reg);
        case AluOperationOpcode::Tst:
        case AluOperationOpcode::Cmp:
        case AluOperationOpcode::Cmn:
          return with_registers(arm_instruction, 0, dest_reg, operand_reg);
          // return arm_instruction | (dest_reg << 16) | operand_reg;
        case AluOperationOpcode::Neg:
          // Set an immediate operand of 0
          return arm_instruction | (1 << 25) | (operand_reg << 16) |
                 (dest_reg << 12);
        case AluOperationOpcode::Mvn:
          return arm_instruction | (dest_reg << 12) | operand_reg;
        case AluOperationOpcode::Mul: {
          const u32 multiply_instruction{
              0b1110'000000'0'1'0000'0000'0000'1001'0000 | (dest_reg << 16) |
              (dest_reg << 8) | operand_reg

          };
          return multiply_instruction;
        }
      }

    }

    // const u32 arm_instruction = 0b1110'001'1101'0000'0000'0000'0000 | offset;
    break;
    case ThumbInstructionType::HiRegisterOperation: {
      enum class HiRegisterOperationOpcode : u32 {
        Add = 0b00,
        Cmp = 0b01,
        Mov = 0b10,
        Bx = 0b11
      };
      const bool hi_one = test_bit(instruction, 7);
      const bool hi_two = test_bit(instruction, 6);

      const u32 adjusted_dest = hi_one ? dest_reg + 8 : dest_reg;
      const u32 adjusted_operand = hi_two ? operand_reg + 8 : operand_reg;
      const auto opcode =
          static_cast<HiRegisterOperationOpcode>((instruction >> 8) & 0b11);

      switch (opcode) {
        case HiRegisterOperationOpcode::Add: {
          constexpr u32 base_instruction =
              make_data_processing(Opcode::Add, false, false);
#if 0
          const u32 arm_instruction{base_instruction | (adjusted_dest << 16) |
                                    (adjusted_dest << 12) | adjusted_operand};
#endif
          return with_registers(base_instruction, adjusted_dest, adjusted_dest,
                                adjusted_operand);
        }

        case HiRegisterOperationOpcode::Cmp: {
          constexpr u32 base_instruction =
              make_data_processing(Opcode::Cmp, false, true);
          return with_registers(base_instruction, 0, adjusted_dest,
                                adjusted_operand);
        }

        break;
        case HiRegisterOperationOpcode::Mov: {
          constexpr u32 base_instruction =
              make_data_processing(Opcode::Mov, false, false);
          return with_registers(base_instruction, adjusted_dest, 0,
                                adjusted_operand);
        }

        case HiRegisterOperationOpcode::Bx: {
          const u32 arm_instruction{0b1110'0001'0010'1111'1111'1111'0001'0000 |
                                    adjusted_operand};
          return arm_instruction;
        }
      }
      break;
    }
    case ThumbInstructionType::PcRelativeLoad: {
      const u32 offset = instruction & 0xff;
      const u32 dest_reg_override = (instruction >> 8) & 0b111;
      const u32 arm_instruction{0b1110'01'0'1'1'0'0'1'0000'0000'000000000000 |
                                (15 << 16) | (dest_reg_override << 12) |
                                (offset << 2)};
      return arm_instruction;
    }

    case ThumbInstructionType::LoadStoreRegisterOffset: {
      constexpr auto make_single_data_transfer =
          [](bool register_offset, bool preindex, bool add, bool byte,
             bool write_back, bool load) {
            return 0b1110'01'0'0'0'0'0'0'0000'0000'000000000000 |
                   (register_offset ? 1 << 25 : 0) | (preindex ? 1 << 24 : 0) |
                   (add ? 1 << 23 : 0) | (byte ? 1 << 22 : 0) |
                   (write_back ? 1 << 21 : 0) | (load ? 1 << 20 : 0);
          };

      const u32 load = (instruction & (1 << 11)) << 9;

      const u32 byte = (instruction & (1 << 10)) << 12;

      const u32 base_instruction =
          make_single_data_transfer(true, true, true, false, false, false) |
          load | byte;

      const u32 base_register = operand_reg;
      const u32 offset_reg = (instruction >> 6) & 0b111;

      return with_registers(base_instruction, dest_reg, base_register,
                            offset_reg);
    }
    case ThumbInstructionType::LoadStoreSignExtendedByteHalfword: {
      constexpr auto make_halfword_data_transfer = []() {};

      const u32 opcode = (((instruction & (1 << 10)) >> 9) |
                          ((instruction & (1 << 11)) >> 11)) &
                         0b11;

      const u32 base_instruction{
          0b1110'000'1'1'0'0'0'0000'0000'0000'1'0'0'1'0000};

      static constexpr std::array<u32, 4> opcode_conversion_table = {
          // STRH
          0b01 << 5,
          // LDRH
          (1 << 20) | (0b01 << 5),
          // LDSB
          (1 << 20) | (0b10 << 5),
          // LDSH
          (1 << 20) | (0b11 << 5)};

      const u32 converted_opcode = opcode_conversion_table[opcode];

      return with_registers(base_instruction, dest_reg, operand_reg,
                            (instruction >> 6) & 0b111) |
             converted_opcode;
    }
  }

  fmt::print("got opcode type {}\n", static_cast<u32>(instruction_type));

  return 0;
}

static void check_thumb_equivalent(const std::string& thumb_assembly,
                                   const std::string& arm_assembly) {
  using namespace std::literals;
  const std::string final_thumb_assembly = ".thumb\n"s + thumb_assembly;
  const std::string final_arm_assembly = ".arm\n"s + arm_assembly;

  const auto thumb_code = experiments::assemble(final_thumb_assembly);
  const auto arm_code = experiments::assemble(final_arm_assembly);

  const u16 thumb_instruction =
      *reinterpret_cast<const u16*>(thumb_code.data());

  CAPTURE(thumb_instruction);
  CAPTURE(thumb_assembly);
  CAPTURE(arm_assembly);
  CHECK(thumb_code.size() == 2);
  CHECK(arm_code.size() == 4);
  CHECK(convert_thumb_to_arm(thumb_instruction) ==
        *reinterpret_cast<const u32*>(arm_code.data()));
}

// Based on "ARM equivalent" tables from ARM7TDMI.pdf
TEST_CASE("shift instructions should be correctly translated") {
  CHECK(thumb_lookup_table.decode_instruction_type(0x01d3) ==
        ThumbInstructionType::MoveShiftedRegister);
  check_thumb_equivalent("lsls r1, r2, #4", "movs r1,r2, lsl #4");
  check_thumb_equivalent("lsrs r1, r2, #4", "movs r1,r2, lsr #4");
  check_thumb_equivalent("asrs r1, r2, #4", "movs r1,r2, asr #4");
}

TEST_CASE("add/subtract instructions should be correctly translated") {
  // adds r1, r2, r2
  CHECK(thumb_lookup_table.decode_instruction_type(0x1891) ==
        ThumbInstructionType::AddSubtract);
  check_thumb_equivalent("adds r1, r2, r2", "adds r1, r2, r2");
}

TEST_CASE(
    "move/add/compare/subtract immediate instructions should be correctly "
    "translated") {
  // movs r3, #123
  CHECK(thumb_lookup_table.decode_instruction_type(0x237b) ==
        ThumbInstructionType::MoveCompareAddSubtractImm);
  check_thumb_equivalent("movs r3, #123", "movs r3, #123");
  // CHECK(convert_thumb_to_arm(0x237b) == 0xe3b0307b);

  // cmp r3, #123
  check_thumb_equivalent("cmp r3, #123", "cmp r3, #123");
  // CHECK(convert_thumb_to_arm(0x2b7b) == 0xe353007b);

  // adds r3, r3, #123
  check_thumb_equivalent("adds r3, r3, #123", "adds r3, r3, #123");

  // subs r3, r3, #123
  check_thumb_equivalent("subs r3, r3, #123", "subs r3, r3, #123");
}

TEST_CASE("alu operations should be correctly translated") {
  check_thumb_equivalent("ands r1, r2", "ands r1, r1, r2");
  check_thumb_equivalent("eors r1, r2", "eors r1, r1, r2");

  check_thumb_equivalent("lsls r2, r1", "movs r2, r2, lsl r1");
  check_thumb_equivalent("lsrs r2, r1", "movs r2, r2, lsr r1");
  check_thumb_equivalent("asrs r2, r1", "movs r2, r2, asr r1");
  check_thumb_equivalent("rors r2, r1", "movs r2, r2, ror r1");

  check_thumb_equivalent("adcs r3, r4", "adcs r3, r3, r4");
  check_thumb_equivalent("sbcs r3, r4", "sbcs r3, r3, r4");

  check_thumb_equivalent("tst r2, r3", "tst r2, r3");
  check_thumb_equivalent("negs r2, r3", "rsbs r2, r3, #0");

  check_thumb_equivalent("cmp r2, r3", "cmp r2, r3");
  check_thumb_equivalent("cmn r2, r3", "cmn r2, r3");
  check_thumb_equivalent("orrs r2, r3", "orrs r2, r2, r3");

  check_thumb_equivalent("muls r2, r3", "muls r2, r3, r2");
  check_thumb_equivalent("bics r2, r3", "bics r2, r2, r3");
  check_thumb_equivalent("mvns r2, r3", "mvns r2, r3");
}

TEST_CASE("hi register operations should be correctly translated") {
  // ADD
  check_thumb_equivalent("add r1, r10", "add r1, r1, r10");
  check_thumb_equivalent("add r10, r1", "add r10, r10, r1");
  check_thumb_equivalent("add r10, r10", "add r10, r10, r10");

  // CMP
  check_thumb_equivalent("cmp r10, r1", "cmp r10, r1");
  check_thumb_equivalent("cmp r1, r10", "cmp r1, r10");
  check_thumb_equivalent("cmp r10, r10", "cmp r10, r10");

  // MOV
  check_thumb_equivalent("mov r1, r10", "mov r1,r10");
  check_thumb_equivalent("mov r10, r1", "mov r10,r1");
  check_thumb_equivalent("mov r10, r10", "mov r10,r10");

  // BX
  check_thumb_equivalent("bx r4", "bx r4");
  check_thumb_equivalent("bx r14", "bx r14");
}

TEST_CASE("pc relative loads should be correctly translated") {
  check_thumb_equivalent("ldr r3, [pc, #128]", "ldr r3, [r15, #128]");
}

TEST_CASE("loads/stores with register offset should be correctly translated") {
  check_thumb_equivalent("str r4,[r3, r2]", "str r4,[r3,r2]");
  check_thumb_equivalent("strb r4,[r3, r2]", "strb r4,[r3,r2]");
  check_thumb_equivalent("ldr r4,[r3, r2]", "ldr r4,[r3,r2]");
  check_thumb_equivalent("ldrb r4,[r3, r2]", "ldrb r4,[r3,r2]");
}

TEST_CASE(
    "load/store sign-extended byte/halfword should be correctly translated") {
  check_thumb_equivalent("strh r3, [r4,r5]", "strh r3, [r4,r5]");
  check_thumb_equivalent("ldrh r3, [r4,r5]", "ldrh r3, [r4,r5]");
  check_thumb_equivalent("ldrsb r3, [r4,r5]", "ldrsb r3, [r4,r5]");
  check_thumb_equivalent("ldrsh r3, [r4,r5]", "ldrsh r3, [r4,r5]");
}

}  // namespace gb::advance
