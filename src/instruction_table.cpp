#include <functional>
#include "cpu.h"

#define A Cpu::A
#define B Cpu::B
#define C Cpu::C
#define D Cpu::D
#define E Cpu::E
#define F Cpu::F
#define H Cpu::H
#define L Cpu::L
#define BC Cpu::BC
#define DE Cpu::DE
#define HL Cpu::HL

namespace gb {

static std::array<Instruction, 256> bind_instructions(Cpu* cpu) {
  std::array<Instruction, 256> instructions = {{
      {"NOP", 1, 4, std::bind(&Cpu::noop, cpu)},                        // 0x0
      {"LD BC,d16", 3, 12, std::bind(&Cpu::ld_r16_d16, cpu, Cpu::BC)},  // 0x1
      {"LD (BC),A", 1, 8, std::bind(&Cpu::ld_r16_a, cpu, Cpu::BC)},     // 0x2
      {"INC BC", 1, 8, std::bind(&Cpu::inc_r16, cpu, Cpu::BC)},         // 0x3
      {"INC B", 1, 4, std::bind(&Cpu::inc_r8, cpu, Cpu::B)},            // 0x4
      {"DEC B", 1, 4, std::bind(&Cpu::dec_r8, cpu, Cpu::B)},            // 0x5
      {"LD B,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, Cpu::B)},        // 0x6
      {"RLCA", 1, 4, std::bind(&Cpu::rlca, cpu)},                       // 0x7
      {"LD (a16),SP", 3, 20, std::bind(&Cpu::ld_d16_sp, cpu)},          // 0x8
      {"ADD HL,BC", 1, 8, std::bind(&Cpu::add_hl_r16, cpu, Cpu::BC)},   // 0x9
      {"LD A,(BC)", 1, 8, std::bind(&Cpu::ld_a_r16, cpu, Cpu::BC)},     // 0xa
      {"DEC BC", 1, 8, std::bind(&Cpu::dec_r16, cpu, Cpu::BC)},         // 0xb
      {"INC C", 1, 4, std::bind(&Cpu::inc_r8, cpu, Cpu::C)},            // 0xc
      {"DEC C", 1, 4, std::bind(&Cpu::dec_r8, cpu, Cpu::C)},            // 0xd
      {"LD C,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, Cpu::C)},        // 0xe
      {"RRCA", 1, 4, std::bind(&Cpu::rrca, cpu)},                       // 0xf
      {"STOP 0", 2, 4, std::bind(&Cpu::stop, cpu)},                     // 0x10
      {"LD DE,d16", 3, 12, std::bind(&Cpu::ld_r16_d16, cpu, Cpu::DE)},  // 0x11
      {"LD (DE),A", 1, 8, std::bind(&Cpu::ld_r16_a, cpu, Cpu::DE)},     // 0x12
      {"INC DE", 1, 8, std::bind(&Cpu::inc_r16, cpu, Cpu::DE)},         // 0x13
      {"INC D", 1, 4, std::bind(&Cpu::inc_r8, cpu, Cpu::D)},            // 0x14
      {"DEC D", 1, 4, std::bind(&Cpu::dec_r8, cpu, Cpu::D)},            // 0x15
      {"LD D,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, Cpu::D)},        // 0x16
      {"RLA", 1, 4, std::bind(&Cpu::rl_a, cpu)},                        // 0x17
      {"JR r8", 2, 12, std::bind(&Cpu::jr_e8, cpu)},                    // 0x18
      {"ADD HL,DE", 1, 8, std::bind(&Cpu::add_hl_r16, cpu, DE)},        // 0x19
      {"LD A,(DE)", 1, 8, std::bind(&Cpu::ld_a_r16, cpu, DE)},          // 0x1a
      {"DEC DE", 1, 8, std::bind(&Cpu::dec_r16, cpu, DE)},              // 0x1b
      {"INC E", 1, 4, std::bind(&Cpu::inc_r8, cpu, E)},                 // 0x1c
      {"DEC E", 1, 4, std::bind(&Cpu::dec_r8, cpu, E)},                 // 0x1d
      {"LD E,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, E)},             // 0x1e
      {"RRA", 1, 4, std::bind(&Cpu::rra, cpu)},                         // 0x1f
      {"JR NZ,r8", 2, 12, std::bind(&Cpu::jr_cc_e8, cpu)},              // 0x20
      {"LD HL,d16", 3, 12, std::bind(&Cpu::ld_r16_d16, cpu, HL)},       // 0x21
      {"LD (HL+),A", 1, 8, std::bind(&Cpu::ld_hl_inc_a, cpu)},          // 0x22
      {"INC HL", 1, 8, std::bind(&Cpu::inc_r16, cpu, HL)},              // 0x23
      {"INC H", 1, 4, std::bind(&Cpu::inc_r8, cpu, H)},                 // 0x24
      {"DEC H", 1, 4, std::bind(&Cpu::dec_r8, cpu, H)},                 // 0x25
      {"LD H,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, H)},             // 0x26
      {"DAA", 1, 4, std::bind(&Cpu::daa, cpu)},                         // 0x27
      {"JR Z,r8", 2, 12, std::bind(&Cpu::jr_cc_e8, cpu)},               // 0x28
      {"ADD HL,HL", 1, 8, std::bind(&Cpu::add_hl_r16, cpu, HL)},        // 0x29
      {"LD A,(HL+)", 1, 8, std::bind(&Cpu::ld_a_hl_inc, cpu)},          // 0x2a
      {"DEC HL", 1, 8, std::bind(&Cpu::dec_r16, cpu, HL)},              // 0x2b
      {"INC L", 1, 4, std::bind(&Cpu::inc_r8, cpu, L)},                 // 0x2c
      {"DEC L", 1, 4, std::bind(&Cpu::dec_r8, cpu, L)},                 // 0x2d
      {"LD L,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, L)},             // 0x2e
      {"CPL", 1, 4, std::bind(&Cpu::cpl, cpu)},                         // 0x2f
      {"JR NC,r8", 2, 12, std::bind(&Cpu::jr_cc_e8, cpu)},              // 0x30
      {"LD SP,d16", 3, 12, std::bind(&Cpu::ld_sp_d16, cpu)},            // 0x31
      {"LD (HL-),A", 1, 8, std::bind(&Cpu::ld_hl_dec_a, cpu)},          // 0x32
      {"INC SP", 1, 8, std::bind(&Cpu::inc_sp, cpu)},                   // 0x33
      {"INC (HL)", 1, 12, std::bind(&Cpu::inc_hl, cpu)},                // 0x34
      {"DEC (HL)", 1, 12, std::bind(&Cpu::dec_hl, cpu)},                // 0x35
      {"LD (HL),d8", 2, 12, std::bind(&Cpu::ld_hl_d8, cpu)},            // 0x36
      {"SCF", 1, 4, std::bind(&Cpu::scf, cpu)},                         // 0x37
      {"JR C,r8", 2, 12, std::bind(&Cpu::jr_cc_e8, cpu)},               // 0x38
      {"ADD HL,SP", 1, 8, std::bind(&Cpu::add_hl_sp, cpu)},             // 0x39
      {"LD A,(HL-)", 1, 8, std::bind(&Cpu::ld_a_hl_dec, cpu)},          // 0x3a
      {"DEC SP", 1, 8, std::bind(&Cpu::dec_sp, cpu)},                   // 0x3b
      {"INC A", 1, 4, std::bind(&Cpu::inc_r8, cpu, A)},                 // 0x3c
      {"DEC A", 1, 4, std::bind(&Cpu::dec_r8, cpu, A)},                 // 0x3d
      {"LD A,d8", 2, 8, std::bind(&Cpu::ld_r8_d8, cpu, A)},             // 0x3e
      {"CCF", 1, 4, std::bind(&Cpu::ccf, cpu)},                         // 0x3f
      {"LD B,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, B)},           // 0x40
      {"LD B,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, C)},           // 0x41
      {"LD B,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, D)},           // 0x42
      {"LD B,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, E)},           // 0x43
      {"LD B,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, H)},           // 0x44
      {"LD B,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, L)},           // 0x45
      {"LD B,(HL)", 1, 8, std::bind(&Cpu::ld_r8_hl, cpu, B)},           // 0x46
      {"LD B,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, B, A)},           // 0x47
      {"LD C,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, B)},           // 0x48
      {"LD C,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, C)},           // 0x49
      {"LD C,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, D)},           // 0x4a
      {"LD C,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, E)},           // 0x4b
      {"LD C,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, H)},           // 0x4c
      {"LD C,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, L)},           // 0x4d
      {"LD C,(HL)", 1, 8, std::bind(&Cpu::ld_r8_hl, cpu, C)},           // 0x4e
      {"LD C,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, C, A)},           // 0x4f
      {"LD D,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, B)},           // 0x50
      {"LD D,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, C)},           // 0x51
      {"LD D,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, D)},           // 0x52
      {"LD D,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, E)},           // 0x53
      {"LD D,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, H)},           // 0x54
      {"LD D,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, L)},           // 0x55
      {"LD D,(HL)", 1, 8, std::bind(&Cpu::ld_r8_hl, cpu, D)},           // 0x56
      {"LD D,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, D, A)},           // 0x57
      {"LD E,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, B)},           // 0x58
      {"LD E,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, C)},           // 0x59
      {"LD E,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, D)},           // 0x5a
      {"LD E,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, E)},           // 0x5b
      {"LD E,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, H)},           // 0x5c
      {"LD E,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, L)},           // 0x5d
      {"LD E,(HL)", 1, 8, std::bind(&Cpu::ld_r8_hl, cpu, E)},           // 0x5e
      {"LD E,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, E, A)},           // 0x5f
      {"LD H,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, B)},           // 0x60
      {"LD H,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, C)},           // 0x61
      {"LD H,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, D)},           // 0x62
      {"LD H,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, E)},           // 0x63
      {"LD H,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, H)},           // 0x64
      {"LD H,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, L)},           // 0x65
      {"LD H,(HL)", 1, 8, std::bind(&Cpu::ld_r8_hl, cpu, H)},           // 0x66
      {"LD H,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, H, A)},           // 0x67
      {"LD L,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, B)},           // 0x68
      {"LD L,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, C)},           // 0x69
      {"LD L,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, D)},           // 0x6a
      {"LD L,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, E)},           // 0x6b
      {"LD L,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, H)},           // 0x6c
      {"LD L,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, L)},           // 0x6d
      {"LD L,(HL)", 1, 8, std::bind(&Cpu::ld_r8_hl, cpu, L)},           // 0x6e
      {"LD L,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, L, A)},           // 0x6f
      {"LD (HL),B", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, B)},           // 0x70
      {"LD (HL),C", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, C)},           // 0x71
      {"LD (HL),D", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, D)},           // 0x72
      {"LD (HL),E", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, E)},           // 0x73
      {"LD (HL),H", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, H)},           // 0x74
      {"LD (HL),L", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, L)},           // 0x75
      {"HALT", 1, 4, std::bind(&Cpu::halt, cpu)},                       // 0x76
      {"LD (HL),A", 1, 8, std::bind(&Cpu::ld_hl_r8, cpu, A)},           // 0x77
      {"LD A,B", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, B)},           // 0x78
      {"LD A,C", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, C)},           // 0x79
      {"LD A,D", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, D)},           // 0x7a
      {"LD A,E", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, E)},           // 0x7b
      {"LD A,H", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, H)},           // 0x7c
      {"LD A,L", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, L)},           // 0x7d
      {"LD A,(HL)", 1, 8, std::bind(&Cpu::load_a_hl, cpu)},             // 0x7e
      {"LD A,A", 1, 4, std::bind(&Cpu::ld_r8_r8, cpu, A, A)},           // 0x7f
      {"ADD A,B", 1, 4, std::bind(&Cpu::add_a_r8, cpu, B)},             // 0x80
      {"ADD A,C", 1, 4, std::bind(&Cpu::add_a_r8, cpu, C)},             // 0x81
      {"ADD A,D", 1, 4, std::bind(&Cpu::add_a_r8, cpu, D)},             // 0x82
      {"ADD A,E", 1, 4, std::bind(&Cpu::add_a_r8, cpu, E)},             // 0x83
      {"ADD A,H", 1, 4, std::bind(&Cpu::add_a_r8, cpu, H)},             // 0x84
      {"ADD A,L", 1, 4, std::bind(&Cpu::add_a_r8, cpu, L)},             // 0x85
      {"ADD A,(HL)", 1, 8, std::bind(&Cpu::add_a_hl, cpu)},             // 0x86
      {"ADD A,A", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, A)},       // 0x87
      {"ADC A,B", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, B)},       // 0x88
      {"ADC A,C", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, C)},       // 0x89
      {"ADC A,D", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, D)},       // 0x8a
      {"ADC A,E", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, E)},       // 0x8b
      {"ADC A,H", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, H)},       // 0x8c
      {"ADC A,L", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, L)},       // 0x8d
      {"ADC A,(HL)", 1, 8, std::bind(&Cpu::add_carry_a_hl, cpu)},       // 0x8e
      {"ADC A,A", 1, 4, std::bind(&Cpu::add_carry_a_r8, cpu, A)},       // 0x8f
      {"SUB B", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, B)},               // 0x90
      {"SUB C", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, C)},               // 0x91
      {"SUB D", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, D)},               // 0x92
      {"SUB E", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, E)},               // 0x93
      {"SUB H", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, H)},               // 0x94
      {"SUB L", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, L)},               // 0x95
      {"SUB (HL)", 1, 8, std::bind(&Cpu::sub_a_hl, cpu)},               // 0x96
      {"SUB A", 1, 4, std::bind(&Cpu::sub_a_r8, cpu, A)},               // 0x97
      {"SBC A,B", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, B)},             // 0x98
      {"SBC A,C", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, C)},             // 0x99
      {"SBC A,D", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, D)},             // 0x9a
      {"SBC A,E", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, E)},             // 0x9b
      {"SBC A,H", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, H)},             // 0x9c
      {"SBC A,L", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, L)},             // 0x9d
      {"SBC A,(HL)", 1, 8, std::bind(&Cpu::sbc_a_hl, cpu)},             // 0x9e
      {"SBC A,A", 1, 4, std::bind(&Cpu::sbc_a_r8, cpu, A)},             // 0x9f
      {"AND B", 1, 4, std::bind(&Cpu::and_a_r8, cpu, B)},               // 0xa0
      {"AND C", 1, 4, std::bind(&Cpu::and_a_r8, cpu, C)},               // 0xa1
      {"AND D", 1, 4, std::bind(&Cpu::and_a_r8, cpu, D)},               // 0xa2
      {"AND E", 1, 4, std::bind(&Cpu::and_a_r8, cpu, E)},               // 0xa3
      {"AND H", 1, 4, std::bind(&Cpu::and_a_r8, cpu, H)},               // 0xa4
      {"AND L", 1, 4, std::bind(&Cpu::and_a_r8, cpu, L)},               // 0xa5
      {"AND (HL)", 1, 8, std::bind(&Cpu::and_a_hl, cpu)},               // 0xa6
      {"AND A", 1, 4, std::bind(&Cpu::and_a_r8, cpu, A)},               // 0xa7
      {"XOR B", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, B)},               // 0xa8
      {"XOR C", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, C)},               // 0xa9
      {"XOR D", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, D)},               // 0xaa
      {"XOR E", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, E)},               // 0xab
      {"XOR H", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, H)},               // 0xac
      {"XOR L", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, L)},               // 0xad
      {"XOR (HL)", 1, 8, std::bind(&Cpu::xor_a_hl, cpu)},               // 0xae
      {"XOR A", 1, 4, std::bind(&Cpu::xor_a_r8, cpu, A)},               // 0xaf
      {"OR B", 1, 4, std::bind(&Cpu::or_a_r8, cpu, B)},                 // 0xb2
      {"OR C", 1, 4, std::bind(&Cpu::or_a_r8, cpu, C)},                 // 0xb1
      {"OR D", 1, 4, std::bind(&Cpu::or_a_r8, cpu, D)},                 // 0xb2
      {"OR E", 1, 4, std::bind(&Cpu::or_a_r8, cpu, E)},                 // 0xb3
      {"OR H", 1, 4, std::bind(&Cpu::or_a_r8, cpu, H)},                 // 0xb4
      {"OR L", 1, 4, std::bind(&Cpu::or_a_r8, cpu, L)},                 // 0xb5
      {"OR (HL)", 1, 8, std::bind(&Cpu::or_a_hl, cpu)},                 // 0xb6
      {"OR A", 1, 4, std::bind(&Cpu::or_a_r8, cpu, A)},                 // 0xb7
      {"CP B", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, B)},                 // 0xb8
      {"CP C", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, C)},                 // 0xb9
      {"CP D", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, D)},                 // 0xba
      {"CP E", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, E)},                 // 0xbb
      {"CP H", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, H)},                 // 0xbc
      {"CP L", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, L)},                 // 0xbd
      {"CP (HL)", 1, 8, std::bind(&Cpu::cp_a_hl, cpu)},                 // 0xbe
      {"CP A", 1, 4, std::bind(&Cpu::cp_a_r8, cpu, A)},                 // 0xbf
      {"RET NZ", 1, 20, std::bind(&Cpu::ret_conditional, cpu)},         // 0xc0
      {"POP BC", 1, 12, std::bind(&Cpu::pop_r16, cpu, BC)},             // 0xc1
      {"JP NZ,a16", 3, 16, std::bind(&Cpu::jp_cc_n16, cpu)},            // 0xc2
      {"JP a16", 3, 16, std::bind(&Cpu::jp_d16, cpu)},                  // 0xc3
      {"CALL NZ,a16", 3, 24, std::bind(&Cpu::call_nz, cpu)},            // 0xc4
      {"PUSH BC", 1, 16, std::bind(&Cpu::push_r16, cpu, BC)},           // 0xc5
      {"ADD A,d8", 2, 8, std::bind(&Cpu::add_a_d8, cpu)},               // 0xc6
      {"RST 00H", 1, 16, std::bind(&Cpu::rst, cpu)},                    // 0xc7
      {"RET Z", 1, 20, std::bind(&Cpu::ret_conditional, cpu)},          // 0xc8
      {"RET", 1, 16, std::bind(&Cpu::ret, cpu)},                        // 0xc9
      {"JP Z,a16", 3, 16, std::bind(&Cpu::jp_cc_n16, cpu)},             // 0xca
      {"PREFIX CB", 1, 4, std::bind(&Cpu::noop, cpu)},                  // 0xcb
      {"CALL Z,a16", 3, 24, std::bind(&Cpu::call_z, cpu)},              // 0xcc
      {"CALL a16", 3, 24, std::bind(&Cpu::call, cpu)},                  // 0xcd
      {"ADC A,d8", 2, 8, std::bind(&Cpu::add_carry_a_d8, cpu)},         // 0xce
      {"RST 08H", 1, 16, std::bind(&Cpu::rst, cpu)},                    // 0xcf
      {"RET NC", 1, 20, std::bind(&Cpu::ret_conditional, cpu)},         // 0xd0
      {"POP DE", 1, 12, std::bind(&Cpu::pop_r16, cpu, DE)},             // 0xd1
      {"JP NC,a16", 3, 16, std::bind(&Cpu::jp_cc_n16, cpu)},            // 0xd2
      {"INVALID", 3, 16, std::bind(&Cpu::invalid, cpu)},                // 0xd3
      {"CALL NC,a16", 3, 24, std::bind(&Cpu::call_nc, cpu)},            // 0xd4
      {"PUSH DE", 1, 16, std::bind(&Cpu::push_r16, cpu, DE)},           // 0xd5
      {"SUB d8", 2, 8, std::bind(&Cpu::sub_a_d8, cpu)},                 // 0xd6
      {"RST 10H", 1, 16, std::bind(&Cpu::rst, cpu)},                    // 0xd7
      {"RET C", 1, 20, std::bind(&Cpu::ret_conditional, cpu)},          // 0xd8
      {"RETI", 1, 16, std::bind(&Cpu::reti, cpu)},                      // 0xd9
      {"JP C,a16", 3, 16, std::bind(&Cpu::jp_cc_n16, cpu)},             // 0xda
      {"INVALID", 3, 16, std::bind(&Cpu::invalid, cpu)},                // 0xdb
      {"CALL C,a16", 3, 24, std::bind(&Cpu::call_c, cpu)},              // 0xdc
      {"INVALID", 3, 24, std::bind(&Cpu::invalid, cpu)},                // 0xdd
      {"SBC A,d8", 2, 8, std::bind(&Cpu::sbc_a_d8, cpu)},               // 0xde
      {"RST 18H", 1, 16, std::bind(&Cpu::rst, cpu)},                    // 0xdf
      {"LDH (a8),A", 2, 12, std::bind(&Cpu::ld_offset_a, cpu)},         // 0xe0
      {"POP HL", 1, 12, std::bind(&Cpu::pop_r16, cpu, HL)},             // 0xe1
      // Is this right?
      {"LD (C),A", 2, 8, std::bind(&Cpu::ld_offset_c_a, cpu)},         // 0xe2
      {"INVALID", 2, 8, std::bind(&Cpu::invalid, cpu)},                // 0xe3
      {"INVALID", 2, 8, std::bind(&Cpu::invalid, cpu)},                // 0xe4
      {"PUSH HL", 1, 16, std::bind(&Cpu::push_r16, cpu, HL)},          // 0xe5
      {"AND d8", 2, 8, std::bind(&Cpu::and_a_d8, cpu)},                // 0xe6
      {"RST 20H", 1, 16, std::bind(&Cpu::rst, cpu)},                   // 0xe7
      {"ADD SP,r8", 2, 16, std::bind(&Cpu::add_sp_s8, cpu)},           // 0xe8
      {"JP (HL)", 1, 4, std::bind(&Cpu::jp_hl, cpu)},                  // 0xe9
      {"LD (a16),A", 3, 16, std::bind(&Cpu::ld_d16_a, cpu)},           // 0xea
      {"INVALID", 3, 16, std::bind(&Cpu::invalid, cpu)},               // 0xeb
      {"INVALID", 3, 16, std::bind(&Cpu::invalid, cpu)},               // 0xec
      {"INVALID", 3, 16, std::bind(&Cpu::invalid, cpu)},               // 0xed
      {"XOR d8", 2, 8, std::bind(&Cpu::xor_a_d8, cpu)},                // 0xee
      {"RST 28H", 1, 16, std::bind(&Cpu::rst, cpu)},                   // 0xef
      {"LDH A,(a8)", 2, 12, std::bind(&Cpu::ld_read_offset_d8, cpu)},  // 0xf0
      {"POP AF", 1, 12, std::bind(&Cpu::pop_r16, cpu, AF)},            // 0xf1
      {"LD A,(C)", 2, 8, std::bind(&Cpu::ld_read_offset_c, cpu)},      // 0xf2
      {"DI", 1, 4, std::bind(&Cpu::disable_interrupts, cpu)},          // 0xf3
      {"INVALID", 1, 4, std::bind(&Cpu::invalid, cpu)},                // 0xf4
      {"PUSH AF", 1, 16, std::bind(&Cpu::push_af, cpu)},               // 0xf5
      {"OR d8", 2, 8, std::bind(&Cpu::or_a_d8, cpu)},                  // 0xf6
      {"RST 30H", 1, 16, std::bind(&Cpu::rst, cpu)},                   // 0xf7
      {"LD HL,SP+r8", 2, 12, std::bind(&Cpu::ld_hl_sp_s8, cpu)},       // 0xf8
      {"LD SP,HL", 1, 8, std::bind(&Cpu::ld_sp_hl, cpu)},              // 0xf9
      {"LD A,(a16)", 3, 16, std::bind(&Cpu::invalid, cpu)},            // 0xfa
      {"EI", 1, 4, std::bind(&Cpu::enable_interrupts, cpu)},           // 0xfb
      {"INVALID", 1, 4, std::bind(&Cpu::invalid, cpu)},                // 0xfc
      {"INVALID", 1, 4, std::bind(&Cpu::invalid, cpu)},                // 0xfd
      {"CP d8", 2, 8, std::bind(&Cpu::cp_a_d8, cpu)},                  // 0xfe
      {"RST 38H", 1, 16, std::bind(&Cpu::rst, cpu)}                    // 0xff

  }};

  return instructions;
}

InstructionTable::InstructionTable(Cpu& cpu)
    : cpu(&cpu), instructions(bind_instructions(&cpu)) {}

}  // namespace gb
