#include <SDL2/SDL.h>
#include <switch.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include "emulator.h"

extern "C" void __nx_win_exit();

namespace fs = std::filesystem;

static std::vector<std::filesystem::directory_entry> list_directory(
    const std::string& path) {
  const auto iterator = std::filesystem::directory_iterator(path);

  return std::vector<std::filesystem::directory_entry>(
      iterator, std::filesystem::directory_iterator{});
}

class FilePicker {
 public:
  FilePicker(const std::string& initial_path, std::size_t size)
      : window_size{size},
        entries{list_directory(initial_path)},
        initial_directory{initial_path},
        end{std::min(entries.size() - 1, size)} {
    reset_window_bounds();
  }

  const fs::directory_entry& file_selected() const {
    return entries[selected_entry];
  }

  void increment_file_selected() {
    if (selected_entry + 1 < entries.size()) {
      selected_entry++;
      increment_window();
      render();
    }
  }

  void decrement_file_selected() {
    if (selected_entry != 0 && selected_entry - 1 >= 0) {
      selected_entry--;
      decrement_window();
      render();
    }
  }

  void push_directory(const fs::path& directory) {
    path_stack.emplace_back(directory.filename());
    change_directory(compute_current_path());
  }

  void pop_directory() {
    if (path_stack.size() > 0) {
      path_stack.pop_back();
      change_directory(compute_current_path());
    }
  }

  void clear() {
    for (std::size_t i = 0; i < window_size; ++i) {
      std::cout << "\33[2K\n";
    }
    std::cout << "\x1b[0;0H";
  }

  void render() {
    std::for_each(entries.begin() + begin, window_end(),
                  [i = begin, this](const fs::directory_entry& file) mutable {
                    std::cout << "\33[2K" << (i++ == selected_entry ? "->" : "")
                              << file.path().filename() << "\n";
                  });
    std::cout << "\x1b[0;0H";
  }

 private:
  void reset_window_bounds() {
    begin = 0;
    end = std::min(entries.size() - 1, window_size);
    selected_entry = 0;
  }

  void increment_window() {
    if (selected_entry > end && end + 1 < entries.size()) {
      ++end;
      ++begin;
    }
  }

  void decrement_window() {
    if (selected_entry < begin && begin != 0 && begin - 1 >= 0) {
      --begin;
      --end;
    }
  }

  std::vector<fs::directory_entry>::iterator window_end() {
    auto iterator = entries.begin() + begin + window_size + 1;
    return std::min(entries.end(), iterator);
  }

  fs::path compute_current_path() {
    return std::accumulate(
        path_stack.begin(), path_stack.end(), initial_directory,
        [](const fs::path& accum, const std::string& path_part) {
          return accum / path_part;
        });
  }
  void change_directory(const fs::path& directory) {
    entries = list_directory(directory);
    reset_window_bounds();
    clear();
    render();
  }
  std::size_t window_size;
  std::vector<fs::directory_entry> entries;
  fs::path initial_directory;

  std::vector<std::string> path_stack;

  std::size_t selected_entry = 0;
  std::size_t begin = 0;
  std::size_t end = 0;
};

std::string pick_file() {
  FilePicker picker{"/", 15};

  picker.render();

  while (appletMainLoop()) {
    hidScanInput();

    u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

    if (kDown & KEY_UP) {
      picker.decrement_file_selected();
    } else if (kDown & KEY_DOWN) {
      picker.increment_file_selected();
    } else if (kDown & KEY_A) {
      const auto& current_entry = picker.file_selected();
      if (current_entry.is_directory()) {
        picker.push_directory(current_entry);
      } else if (current_entry.is_regular_file()) {
        return picker.file_selected().path();
      }
    } else if (kDown & KEY_B) {
      picker.pop_directory();
    }

    consoleUpdate(NULL);
  }
}

int main(int argc, char** argv) {
  consoleInit(nullptr);
  std::string rom_path = pick_file();
  __nx_win_exit();
  socketInitializeDefault();
  nxlinkStdio();
  std::cout.rdbuf(std::cerr.rdbuf());
  try {
    gb::run_with_options(rom_path, false, false);
  } catch (std::exception& e) {
    const char* error = e.what();
    std::cerr << error << '\n';
  }
  return 0;
}
