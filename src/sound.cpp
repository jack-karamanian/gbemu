#include <SDL2/SDL.h>
#include <array>
#include <iostream>
#include "memory.h"
#include "registers/sound.h"
#include "sound.h"

constexpr std::array<std::array<bool, 8>, 4> DUTY_CYCLES = {{
    {false, false, false, false, false, false, false, true},
    {true, false, false, false, false, false, false, true},
    {true, false, false, false, false, true, true, true},
    {false, true, true, true, true, true, true, false},
}};

namespace gb {
Sound::Sound(Memory& memory) : memory{&memory}, sample_buffer(4096) {}

void Sound::handle_memory_write(u16 addr, u8 value) {
  SquareChannel& square =
      addr <= Registers::Sound::Square1::NR14::Address ? square1 : square2;
  switch (addr) {
    case Registers::Sound::Square1::NR11::Address:
    case Registers::Sound::Square2::NR21::Address:
      square.set_duty_cycle(DUTY_CYCLES.at((value & 0xC0) >> 6));
      square.set_length(64 - (value & 0x3f));
      // TODO: Length load
      break;
    case Registers::Sound::Square1::NR13::Address:
      break;

    case Registers::Sound::Square1::NR14::Address:
    case Registers::Sound::Square2::NR24::Address: {
      if ((value & 0x80) != 0) {
        const u16 lsb_addr = addr == Registers::Sound::Square1::NR14::Address
                                 ? Registers::Sound::Square1::NR13::Address
                                 : Registers::Sound::Square2::NR23::Address;
        const u16 frequency = (value & 0x07) << 8 | memory->get_ram(lsb_addr);
        square.set_length_enabled((value & 0x40) != 0);
        square.set_timer_base(frequency);
        square.enable();
      }

      break;
    }
  }
}

void Sound::update(int ticks) {
  // sample_ticks += ticks;
  // sequencer_ticks += ticks;

  for (int i = 0; i < ticks; i++) {
    float square1_sample = square1.update();
    float square2_sample = square2.update();

    if (++sample_ticks > 87) {
      float mixed_sample = 0;

      SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                         reinterpret_cast<Uint8*>(&square1_sample),
                         AUDIO_F32SYS, sizeof(float), 128);
      SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                         reinterpret_cast<Uint8*>(&square2_sample),
                         AUDIO_F32SYS, sizeof(float), 128);

      sample_buffer.emplace_back(mixed_sample);
      sample_ticks = 0;
    }
    if (sample_buffer.size() >= 1024) {
      samples_ready_callback(sample_buffer);
      sample_buffer.clear();
    }

    if (++sequencer_ticks >= 8192) {
      if (sequencer_step % 2 == 0) {
        square1.clock_length();
        square2.clock_length();
      }

      if (sequencer_step == 7) {
        sequencer_step = 0;
      } else {
        sequencer_step++;
      }

      sequencer_ticks = 0;
    }
  }

  // > 8192
}
}  // namespace gb
