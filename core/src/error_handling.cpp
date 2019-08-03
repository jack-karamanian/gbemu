#include <iostream>
#include "error_handling.h"
#include "fmt/printf.h"

namespace gb {
void handle_unreachable(int line, const char* file, const char* msg) {
  fmt::print(std::cerr, "Unreachable code at {}:{}\n{}", file, line, msg);
  std::terminate();
}
}  // namespace gb
