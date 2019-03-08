#include <SDL2/SDL.h>
#include <array>
#include <iostream>
#include "memory.h"
#include "registers/sound.h"
#include "sound.h"

namespace gb {
Sound::Sound(Memory& memory)
    : memory{&memory},
      wave_channel{{memory.get_range({0xff30, 0xff3f})}},
      sample_buffer(4096) {}

float Sound::mix_samples(AudioFrame& frame, const OutputControl& control) {
  float mixed_sample = 0.0f;
  if (control.square1) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<Uint8*>(&frame.square1_sample),
                       AUDIO_F32SYS, sizeof(float), 128);
  }
  if (control.square2) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<Uint8*>(&frame.square2_sample),
                       AUDIO_F32SYS, sizeof(float), 128);
  }
  if (control.wave) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<Uint8*>(&frame.wave_sample),
                       AUDIO_F32SYS, sizeof(float), 128);
  }
  if (control.noise) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<Uint8*>(&frame.noise_sample),
                       AUDIO_F32SYS, sizeof(float), 128);
  }
  return mixed_sample;
}

void Sound::handle_memory_write(u16 addr, u8 value) {
  auto& square =
      addr <= Registers::Sound::Square1::NR14::Address ? square1 : square2;
  switch (addr) {
    case Registers::Sound::Square1::NR11::Address:
    case Registers::Sound::Square2::NR21::Address:
      square.source.set_duty_cycle((value & 0xC0) >> 6);
      square.dispatch(SetLengthCommand{value & 0x3f});
      break;
    case Registers::Sound::Square1::NR12::Address:
    case Registers::Sound::Square2::NR22::Address:
      square.dispatch(SetStartingVolumeCommand{(value & 0xf0) >> 4});
      square.dispatch(SetIncreaseVolumeCommand{(value & 0x08) != 0});
      square.dispatch(SetPeriodCommand{value & 0x07});
      break;
    case Registers::Sound::Square1::NR13::Address:
      break;
    case Registers::Sound::Square1::NR14::Address:
    case Registers::Sound::Square2::NR24::Address: {
      const u16 lsb_addr = addr - 1;
      const u16 frequency = (value & 0x07) << 8 | memory->get_ram(lsb_addr);
      square.source.set_timer_base(frequency);
      if ((value & 0x80) != 0) {
        square.enable();
      }

      break;
    }
    case Registers::Sound::Wave::NR31::Address:
      wave_channel.dispatch(SetLengthCommand{value});
      break;
    case Registers::Sound::Wave::NR32::Address:
      wave_channel.dispatch(VolumeShiftCommand{(value & 0x60) >> 5});
      break;
    case Registers::Sound::Wave::NR33::Address: {
      const u16 frequency = (memory->get_ram(addr + 1) & 0x07) << 8 | value;
      wave_channel.source.set_timer_base(frequency);
      break;
    }
    case Registers::Sound::Wave::NR34::Address: {
      const u16 lsb_addr = addr - 1;
      const u16 frequency = (value & 0x07) << 8 | memory->get_ram(lsb_addr);
      wave_channel.source.set_timer_base(frequency);
      wave_channel.dispatch(SetLengthEnabledCommand{(value & 0x40) != 0});
      if ((value & 0x80) != 0) {
        wave_channel.enable();
      }

      break;
    }

    case Registers::Sound::Noise::NR41::Address:
      noise_channel.dispatch(SetLengthCommand{value & 0x3f});
      break;
    case Registers::Sound::Noise::NR42::Address:
      noise_channel.dispatch(SetStartingVolumeCommand{(value & 0xf0) >> 4});
      noise_channel.dispatch(SetIncreaseVolumeCommand{(value & 0x8) != 0});
      noise_channel.dispatch(SetPeriodCommand{value & 0x7});
      break;
    case Registers::Sound::Noise::NR43::Address:
      noise_channel.source.set_prescalar_divider((value & 0xf0) >> 4);
      noise_channel.source.set_num_stages((value & 0x8) != 0);
      noise_channel.source.set_clock_divisor(value & 0x7);
      break;

    case Registers::Sound::Noise::NR44::Address: {
      noise_channel.dispatch(SetLengthEnabledCommand{(value & 0x40) != 0});
      if ((value & 0x80) != 0) {
        noise_channel.enable();
      }
      break;
    }

    case Registers::Sound::Control::NR51::Address:
      right_output.set_enabled(value & 0xf);
      left_output.set_enabled((value & 0xf0) >> 4);
      break;

    case Registers::Sound::Control::NR52::Address: {
      if ((value & 0x01) != 0) {
        square1.enable();
      } else {
        square1.disable();
      }
      if ((value & 0x02) != 0) {
        square2.enable();
      } else {
        square2.disable();
      }
      if ((value & 0x04) != 0) {
        wave_channel.enable();
      } else {
        wave_channel.disable();
      }

      if ((value & 0x80) != 0) {
        // noise_channel.enable();
        printf("enable noise\n");
      } else {
        printf("disable noise\n");
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
    float wave_sample = wave_channel.update();
    float noise_sample = noise_channel.update();

    if (++sample_ticks > 87) {
      AudioFrame frame{square1_sample, square2_sample, wave_sample,
                       noise_sample};

      float left_sample = mix_samples(frame, left_output);
      float right_sample = mix_samples(frame, right_output);

      sample_buffer.emplace_back(left_sample);
      sample_buffer.emplace_back(right_sample);

      if (sample_buffer.size() >= 4096) {
        samples_ready_callback(sample_buffer);
        sample_buffer.clear();
      }
      sample_ticks = 0;
    }

    if (++sequencer_ticks >= 8192) {
      square1.clock(sequencer_step);
      square2.clock(sequencer_step);
      wave_channel.clock(sequencer_step);
      noise_channel.clock(sequencer_step);

      if (sequencer_step == 7) {
        sequencer_step = 0;
      } else {
        sequencer_step++;
      }

      sequencer_ticks = 0;
    }
  }
}
}  // namespace gb
