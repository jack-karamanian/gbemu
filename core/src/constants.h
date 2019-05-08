#pragma once

namespace gb {
constexpr int SCREEN_WIDTH = 160;
constexpr int SCREEN_HEIGHT = 144;
constexpr int DISPLAY_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;

constexpr int TILE_SIZE = 8;

constexpr int CLOCK_FREQUENCY = 4194304;
#ifdef __SWITCH__
constexpr int SOUND_SAMPLE_FREQUENCY = 48000;
#else
constexpr int SOUND_SAMPLE_FREQUENCY = 44100;
#endif
}  // namespace gb
