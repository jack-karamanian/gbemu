#include "registers/sound.h"
#include <SDL2/SDL.h>
#include <fmt/ostream.h>
#include <array>
#include <numeric>
#include <thread>
#include "memory.h"
#include "sound.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace gb {
static float dac_output(u8 volume) {
  return static_cast<float>(volume) / 15.0f;
}

Sound::Sound(Memory& memory, SDL_AudioDeviceID device)
    : memory{&memory},
      square1{{true}},
      square2{{false}},
      wave_channel{{memory.get_range({0xff30, 0xff3f})}},
      audio_device{device} {
  sample_buffer.reserve(4096);
  noise_samples.reserve(95);
}

float Sound::mix_samples(const AudioFrame& frame,
                         const OutputControl& control) const {
  float mixed_sample = 0.0f;

  const float square1_sample = dac_output(frame.square1_sample);
  const float square2_sample = dac_output(frame.square2_sample);
  const float wave_sample = dac_output(frame.wave_sample);
  const float noise_sample = dac_output(frame.noise_sample);

  int output_volume = ((control.volume * 128) / 16);
  if (control.square1) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<const Uint8*>(&square1_sample),
                       AUDIO_F32SYS, sizeof(float), output_volume);
  }
  if (control.square2) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<const Uint8*>(&square2_sample),
                       AUDIO_F32SYS, sizeof(float), output_volume);
  }
  if (control.wave) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<const Uint8*>(&wave_sample),
                       AUDIO_F32SYS, sizeof(float), output_volume);
  }
  if (control.noise) {
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<const Uint8*>(&noise_sample),
                       AUDIO_F32SYS, sizeof(float), output_volume);
  }
  return mixed_sample * 0.032f;
}

u8 Sound::handle_memory_read(u16 addr) const {
  switch (addr) {
    case Registers::Sound::Control::NR52::Address: {
      const u8 square1_enabled = square1.is_enabled() ? 1 : 0;
      const u8 square2_enabled = square2.is_enabled() ? 1 : 0;
      const u8 wave_enabled = wave_channel.is_enabled() ? 1 : 0;
      const u8 noise_enabled = noise_channel.is_enabled() ? 1 : 0;

      return (sound_power_on ? (1 << 7) : 0) | (noise_enabled << 3) |
             (wave_enabled << 2) | (square2_enabled << 1) | (square1_enabled);
    }
    default:
      return 0;
  }
}

void Sound::handle_memory_write(u16 addr, u8 value) {
  auto& square =
      addr <= Registers::Sound::Square1::NR14::Address ? square1 : square2;
  switch (addr) {
    // Square
    case Registers::Sound::Square1::NR10::Address:
      square1.source.set_sweep_period((value & 0x70) >> 4);
      square1.source.set_sweep_negate((value & 0x8) != 0);
      square1.source.set_sweep_shift(value & 0x7);
      break;
    case Registers::Sound::Square1::NR11::Address:
    case Registers::Sound::Square2::NR21::Address:
      square.source.set_duty_cycle((value & 0xc0) >> 6);
      square.dispatch(SetLengthCommand{value & 0x3f});
      break;
    case Registers::Sound::Square1::NR12::Address:
    case Registers::Sound::Square2::NR22::Address:
      square.dispatch(SetStartingVolumeCommand{(value & 0xf0) >> 4});
      square.dispatch(SetIncreaseVolumeCommand{(value & 0x08) != 0});
      square.dispatch(SetPeriodCommand{value & 0x07});
      if (((value & 0xf8)) == 0) {
        square.disable();
      }
      break;
    case Registers::Sound::Square1::NR13::Address:
    case Registers::Sound::Square2::NR23::Address: {
      const u16 frequency = (memory->get_ram(addr + 1) & 0x07) << 8 | value;
      square.source.set_timer_base(frequency);
      break;
    }
    case Registers::Sound::Square1::NR14::Address:
    case Registers::Sound::Square2::NR24::Address: {
      const u16 lsb_addr = addr - 1;
      const u16 frequency = (value & 0x07) << 8 | memory->get_ram(lsb_addr);
      square.source.set_timer_base(frequency);
      square.dispatch(SetLengthEnabledCommand{(value & 0x40) != 0});
      if ((value & 0x80) != 0) {
        square.enable();
      }

      break;
    }
      // Wave
    case Registers::Sound::Wave::NR30::Address:
      if (((value & 0xf8)) == 0) {
        wave_channel.disable();
      }
      break;

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

    // Noise
    case Registers::Sound::Noise::NR41::Address:
      noise_channel.dispatch(SetLengthCommand{value & 0x3f});
      break;
    case Registers::Sound::Noise::NR42::Address:
      noise_channel.dispatch(SetStartingVolumeCommand{(value & 0xf0) >> 4});
      noise_channel.dispatch(SetIncreaseVolumeCommand{(value & 0x8) != 0});
      noise_channel.dispatch(SetPeriodCommand{(value & 0x7)});
      // std::cout << "length " << ((value & 0x7) >> 0) << '\n';
      if (((value & 0xf8)) == 0) {
        noise_channel.disable();
      }
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

    case Registers::Sound::Control::NR50::Address:
      left_output.volume = (value & 0x70) >> 4;
      right_output.volume = (value & 0x7);
      break;

    case Registers::Sound::Control::NR51::Address:
      right_output.set_enabled(value & 0xf);
      left_output.set_enabled((value & 0xf0) >> 4);
      break;

    case Registers::Sound::Control::NR52::Address: {
      if (!test_bit(value, 7)) {
        sound_power_on = false;
        square1.disable();
        square2.disable();
        wave_channel.disable();
        noise_channel.disable();
      } else {
        sound_power_on = true;
      }
      break;
    }
  }
}

void Sound::update(int ticks) {
  sample_ticks += ticks;
  noise_channel.update(ticks);
  const u8 noise_volume = noise_channel.volume();
  noise_samples.push_back(noise_volume);

  square1.update(ticks);
  square2.update(ticks);
  wave_channel.update(ticks);
  samples_task.run(ticks, [&]() {
    sample_ticks = 0;
    const u8 square1_sample = square1.volume();
    const u8 square2_sample = square2.volume();
    const u8 wave_sample = wave_channel.volume();
    const u8 noise_sample =
        std::accumulate(noise_samples.begin(), noise_samples.end(), 0) /
        noise_samples.size();
    noise_samples.clear();
    const AudioFrame frame{square1_sample, square2_sample, wave_sample,
                           noise_sample};

    const float left_sample = mix_samples(frame, left_output);
    const float right_sample = mix_samples(frame, right_output);

    sample_buffer.push_back(left_sample);
    sample_buffer.push_back(right_sample);

    if (sample_buffer.size() >= SOUND_SAMPLE_BUFFER_SIZE * 2 &&
        SDL_GetQueuedAudioSize(audio_device) <
            SOUND_SAMPLE_BUFFER_SIZE * sizeof(float) / 2) {
      if (SDL_QueueAudio(audio_device, sample_buffer.data(),
                         sample_buffer.size() * sizeof(float))) {
        std::cout << "SDL Error: " << SDL_GetError() << std::endl;
      }
      sample_buffer.clear();
      while (SDL_GetQueuedAudioSize(audio_device) >
             SOUND_SAMPLE_BUFFER_SIZE * sizeof(float) * 2) {
        std::this_thread::yield();
      }
    }
  });
#if 1
#endif

  sequencer_task.run(ticks, [&]() {
    square1.clock(sequencer_step);
    square2.clock(sequencer_step);
    wave_channel.clock(sequencer_step);
    noise_channel.clock(sequencer_step);

    if (sequencer_step == 2 || sequencer_step == 6) {
      square1.source.clock_sweep();
    }

    if (sequencer_step == 7) {
      sequencer_step = 0;
    } else {
      sequencer_step++;
    }
  });
}
}  // namespace gb
