#pragma once
#include "types.h"

namespace gb::advance {

namespace hardware {
// LCD Control
constexpr u32 DISPCNT = 0x4000000;
// General LCD Status (STAT,LYC)
constexpr u32 DISPSTAT = 0x4000004;
// Vertical Counter (LY)
constexpr u32 VCOUNT = 0x4000006;
// BG0 Control
constexpr u32 BG0CNT = 0x4000008;
// BG1 Control
constexpr u32 BG1CNT = 0x400000A;
// BG2 Control
constexpr u32 BG2CNT = 0x400000C;
// BG3 Control
constexpr u32 BG3CNT = 0x400000E;
// BG0 X-Offset
constexpr u32 BG0HOFS = 0x4000010;
// BG0 Y-Offset
constexpr u32 BG0VOFS = 0x4000012;
// BG1 X-Offset
constexpr u32 BG1HOFS = 0x4000014;
// BG1 Y-Offset
constexpr u32 BG1VOFS = 0x4000016;
// BG2 X-Offset
constexpr u32 BG2HOFS = 0x4000018;
// BG2 Y-Offset
constexpr u32 BG2VOFS = 0x400001A;
// BG3 X-Offset
constexpr u32 BG3HOFS = 0x400001C;
// BG3 Y-Offset
constexpr u32 BG3VOFS = 0x400001E;
// BG2 Rotation/Scaling Parameter A (dx)
constexpr u32 BG2PA = 0x4000020;
// BG2 Rotation/Scaling Parameter B (dmx)
constexpr u32 BG2PB = 0x4000022;
// BG2 Rotation/Scaling Parameter C (dy)
constexpr u32 BG2PC = 0x4000024;
// BG2 Rotation/Scaling Parameter D (dmy)
constexpr u32 BG2PD = 0x4000026;
// BG2 Reference Point X-Coordinate
constexpr u32 BG2X = 0x4000028;
// BG2 Reference Point Y-Coordinate
constexpr u32 BG2Y = 0x400002C;
// BG3 Rotation/Scaling Parameter A (dx)
constexpr u32 BG3PA = 0x4000030;
// BG3 Rotation/Scaling Parameter B (dmx)
constexpr u32 BG3PB = 0x4000032;
// BG3 Rotation/Scaling Parameter C (dy)
constexpr u32 BG3PC = 0x4000034;
// BG3 Rotation/Scaling Parameter D (dmy)
constexpr u32 BG3PD = 0x4000036;
// BG3 Reference Point X-Coordinate
constexpr u32 BG3X = 0x4000038;
// BG3 Reference Point Y-Coordinate
constexpr u32 BG3Y = 0x400003C;
// Window 0 Horizontal Dimensions
constexpr u32 WIN0H = 0x4000040;
// Window 1 Horizontal Dimensions
constexpr u32 WIN1H = 0x4000042;
// Window 0 Vertical Dimensions
constexpr u32 WIN0V = 0x4000044;
// Window 1 Vertical Dimensions
constexpr u32 WIN1V = 0x4000046;
// Inside of Window 0 and 1
constexpr u32 WININ = 0x4000048;
// Inside of OBJ Window & Outside of Windows
constexpr u32 WINOUT = 0x400004A;
// Mosaic Size
constexpr u32 MOSAIC = 0x400004C;
// Color Special Effects Selection
constexpr u32 BLDCNT = 0x4000050;
// Alpha Blending Coefficients
constexpr u32 BLDALPHA = 0x4000052;
// Brightness (Fade-In/Out) Coefficient
constexpr u32 BLDY = 0x4000054;
// Channel 1 Sweep register (NR10)
constexpr u32 SOUND1CNT_L = 0x4000060;
// Channel 1 Duty/Length/Envelope (NR11, NR12)
constexpr u32 SOUND1CNT_H = 0x4000062;
// Channel 1 Frequency/Control (NR13, NR14)
constexpr u32 SOUND1CNT_X = 0x4000064;
// Channel 2 Duty/Length/Envelope (NR21, NR22)
constexpr u32 SOUND2CNT_L = 0x4000068;
// Channel 2 Frequency/Control (NR23, NR24)
constexpr u32 SOUND2CNT_H = 0x400006C;
// Channel 3 Stop/Wave RAM select (NR30)
constexpr u32 SOUND3CNT_L = 0x4000070;
// Channel 3 Length/Volume (NR31, NR32)
constexpr u32 SOUND3CNT_H = 0x4000072;
// Channel 3 Frequency/Control (NR33, NR34)
constexpr u32 SOUND3CNT_X = 0x4000074;
// Channel 4 Length/Envelope (NR41, NR42)
constexpr u32 SOUND4CNT_L = 0x4000078;
// Channel 4 Frequency/Control (NR43, NR44)
constexpr u32 SOUND4CNT_H = 0x400007C;
// Control Stereo/Volume/Enable (NR50, NR51)
constexpr u32 SOUNDCNT_L = 0x4000080;
// Control Mixing/DMA Control
constexpr u32 SOUNDCNT_H = 0x4000082;
// Control Sound on/off (NR52)
constexpr u32 SOUNDCNT_X = 0x4000084;
// Sound PWM Control
constexpr u32 SOUNDBIAS = 0x4000088;
// Channel A FIFO, Data 0-3
constexpr u32 FIFO_A = 0x40000A0;
// Channel B FIFO, Data 0-3
constexpr u32 FIFO_B = 0x40000A4;
// DMA 0 Source Address
constexpr u32 DMA0SAD = 0x40000B0;
// DMA 0 Destination Address
constexpr u32 DMA0DAD = 0x40000B4;
// DMA 0 Word Count
constexpr u32 DMA0CNT_L = 0x40000B8;
// DMA 0 Control
constexpr u32 DMA0CNT_H = 0x40000BA;
// DMA 1 Source Address
constexpr u32 DMA1SAD = 0x40000BC;
// DMA 1 Destination Address
constexpr u32 DMA1DAD = 0x40000C0;
// DMA 1 Word Count
constexpr u32 DMA1CNT_L = 0x40000C4;
// DMA 1 Control
constexpr u32 DMA1CNT_H = 0x40000C6;
// DMA 2 Source Address
constexpr u32 DMA2SAD = 0x40000C8;
// DMA 2 Destination Address
constexpr u32 DMA2DAD = 0x40000CC;
// DMA 2 Word Count
constexpr u32 DMA2CNT_L = 0x40000D0;
// DMA 2 Control
constexpr u32 DMA2CNT_H = 0x40000D2;
// DMA 3 Source Address
constexpr u32 DMA3SAD = 0x40000D4;
// DMA 3 Destination Address
constexpr u32 DMA3DAD = 0x40000D8;
// DMA 3 Word Count
constexpr u32 DMA3CNT_L = 0x40000DC;
// DMA 3 Control
constexpr u32 DMA3CNT_H = 0x40000DE;
// Timer 0 Counter/Reload
constexpr u32 TM0COUNTER = 0x4000100;
// Timer 0 Control
constexpr u32 TM0CONTROL = 0x4000102;
// Timer 1 Counter/Reload
constexpr u32 TM1COUNTER = 0x4000104;
// Timer 1 Control
constexpr u32 TM1CONTROL = 0x4000106;
// Timer 2 Counter/Reload
constexpr u32 TM2COUNTER = 0x4000108;
// Timer 2 Control
constexpr u32 TM2CONTROL = 0x400010A;
// Timer 3 Counter/Reload
constexpr u32 TM3COUNTER = 0x400010C;
// Timer 3 Control
constexpr u32 TM3CONTROL = 0x400010E;
// SIO Data (Normal-32bit Mode; shared with below)
constexpr u32 SIODATA32 = 0x4000120;
// SIO Data 0 (Parent) (Multi-Player Mode)
constexpr u32 SIOMULTI0 = 0x4000120;
// SIO Data 1 (1st Child) (Multi-Player Mode)
constexpr u32 SIOMULTI1 = 0x4000122;
// SIO Data 2 (2nd Child) (Multi-Player Mode)
constexpr u32 SIOMULTI2 = 0x4000124;
// SIO Data 3 (3rd Child) (Multi-Player Mode)
constexpr u32 SIOMULTI3 = 0x4000126;
// SIO Control Register
constexpr u32 SIOCNT = 0x4000128;
// SIO Data (Local of MultiPlayer; shared below)
constexpr u32 SIOMLT_SEND = 0x400012A;
// SIO Data (Normal-8bit and UART Mode)
constexpr u32 SIODATA8 = 0x400012A;
// Key Status
constexpr u32 KEYINPUT = 0x4000130;
// Key Interrupt Control
constexpr u32 KEYCNT = 0x4000132;
// SIO Mode Select/General Purpose Data
constexpr u32 RCNT = 0x4000134;
// SIO JOY Bus Control
constexpr u32 JOYCNT = 0x4000140;
// SIO JOY Bus Receive Data
constexpr u32 JOY_RECV = 0x4000150;
// SIO JOY Bus Transmit Data
constexpr u32 JOY_TRANS = 0x4000154;
// SIO JOY Bus Receive Status
constexpr u32 JOYSTAT = 0x4000158;
// Interrupt Enable Register
constexpr u32 IE = 0x4000200;
// Interrupt Request Flags / IRQ Acknowledge
constexpr u32 IF = 0x4000202;
// Game Pak Waitstate Control
constexpr u32 WAITCNT = 0x4000204;
// Interrupt Master Enable Register
constexpr u32 IME = 0x4000208;
// Undocumented - Post Boot Flag
constexpr u32 POSTFLG = 0x4000300;
// Undocumented - Power Down Control
constexpr u32 HALTCNT = 0x4000301;
}  // namespace hardware

struct IoRegisterResult {
  u32 addr;
  u32 offset;
};

IoRegisterResult select_io_register(u32 addr);

}  // namespace gb::advance
