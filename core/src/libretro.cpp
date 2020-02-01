#include "libretro.h"
#include <cstdio>
#include <cstring>

unsigned int retro_api_version() {
  return RETRO_API_VERSION;
}

void retro_init() {
  printf("hello world\n");
}

void retro_deinit() {}

void retro_get_system_info(retro_system_info* info) {
  std::memset(info, 0, sizeof(retro_system_info));
}

void retro_get_system_av_info(retro_system_av_info* info) {
  info->geometry.base_width = 160;
  info->geometry.base_height = 144;

  info->geometry.max_width = 160;
  info->geometry.max_height = 144;

  info->geometry.aspect_ratio = 0.0F;
}

void retro_set_environment(retro_environment_t set_env) {}

void retro_set_audio_sample(retro_audio_sample_t cb) {}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {}

void retro_set_input_poll(retro_input_poll_t cb) {}

void retro_set_input_state(retro_input_state_t cb) {}

void retro_set_video_refresh(retro_video_refresh_t cb) {}

void retro_set_controller_port_device(unsigned int port, unsigned int device) {}

void retro_reset() {}

void retro_run() {}

size_t retro_serialize_size() {
  return 0;
}

bool retro_serialize(void* data, size_t size) {
  return false;
}

bool retro_unserialize(const void* data, size_t size) {
  return false;
}

void retro_cheat_reset() {}

void retro_cheat_set(unsigned int index, bool enabled, const char* code) {}

bool retro_load_game(const retro_game_info* game) {
  return false;
}

bool retro_load_game_special(unsigned int type,
                             const retro_game_info* info,
                             size_t num) {
  return false;
}

void retro_unload_game() {}

unsigned int retro_get_region() {
  return RETRO_REGION_NTSC;
}

void* retro_get_memory_data(unsigned int id) {
  return nullptr;
}

size_t retro_get_memory_size(unsigned int id) {
  return 0;
}
