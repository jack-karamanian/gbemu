#pragma once

#ifndef NDEBUG
#define GB_UNREACHABLE() gb::handle_unreachable(__LINE__, __FILE__)
#else
#define GB_UNREACHABLE() __builtin_unreachable()
#endif

namespace gb {
[[noreturn]] void handle_unreachable(int line,
                                     const char* file,
                                     const char* msg = nullptr);
}
