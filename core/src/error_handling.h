#pragma once

#ifndef NDEBUG
#define GB_UNREACHABLE() gb::handle_unreachable(__LINE__, __FILE__)
#else
#ifdef __GNUC__
#define GB_UNREACHABLE() __builtin_unreachable()
#else
namespace gb::error_handling::detail {
[[noreturn]] static inline void unreachable() {}

}  // namespace gb::error_handling::detail
#define GB_UNREACHABLE() gb::error_handling::detail::unreachable()
#endif
#endif

namespace gb {
[[noreturn]] void handle_unreachable(int line,
                                     const char* file,
                                     const char* msg = nullptr);
}
