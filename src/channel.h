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

  u8 update() {
    if (enabled) {
      u8 res = source.update();
      for_static<sizeof...(Mods)>(
          [&res, this](auto i) { res = std::get<i>(mods).update(res); });
      return res;
    }
    return 0;
  }

  void clock(int step) {
    if (enabled) {
      for_static<sizeof...(Mods)>(
          [this, step](auto i) { std::get<i>(mods).clock(step); });
    }
  }

  void enable() {
    enabled = true;
    source.enable();
    for_static<sizeof...(Mods)>([this](auto i) { std::get<i>(mods).enable(); });
  }

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
