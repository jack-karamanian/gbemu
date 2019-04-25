#pragma once
#include <tuple>
#include "types.h"
#include "utils.h"
#include "wave_source.h"

namespace gb {

template <typename Source, typename... Mods>
class Channel {
  template <typename Mod,
            typename Command,
            void (Mod::*Dispatch)(Command) = &Mod::dispatch>
  void dispatch_command(Mod& mod, Command command) {
    mod.dispatch(command);
  }

  void dispatch_command(...) {}

  std::tuple<Mods...> mods;
  bool enabled = false;

 public:
  Source source;

  Channel(Source source) : source(std::move(source)) {}

  Channel() {}

  void update(int ticks) { source.update(ticks); }

  u8 volume() const {
    if (enabled) {
      u8 volume = source.volume();
      for_static<sizeof...(Mods)>([this, &volume](auto i) {
        volume = std::get<i>(mods).update(volume);
      });
      return volume;
    }
    return 0;
  }

  void clock(int step) {
    if (enabled) {
      for_static<sizeof...(Mods)>([this, step](auto i) {
        auto& mod = std::get<i>(mods);
        if constexpr (std::is_same_v<decltype(mod.clock(step)), bool>) {
          enabled = mod.clock(step);
        } else {
          mod.clock(step);
        }
      });
    }
  }

  void enable() {
    enabled = true;
    source.enable();
    for_static<sizeof...(Mods)>([this](auto i) { std::get<i>(mods).enable(); });
  }

  bool is_enabled() const { return enabled; }

  void disable() { enabled = false; }

  template <typename T>
  void dispatch(T command) {
    for_static<sizeof...(Mods)>([this, command = std::move(command)](auto i) {
      auto& mod = std::get<i>(mods);
      dispatch_command(mod, command);
    });
  }
};

}  // namespace gb
