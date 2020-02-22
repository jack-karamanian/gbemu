#pragma once

#ifndef NDEBUG
#define GB_UNREACHABLE() gb::handle_unreachable(__LINE__, __FILE__)
#else
#ifdef __GNUC__
#define GB_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define GB_UNREACHABLE() __assume(0)
#endif
#endif

namespace gb {
[[noreturn]] void handle_unreachable(int line,
                                     const char* file,
                                     const char* msg = nullptr);
}
