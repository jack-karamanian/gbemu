#include <boost/program_options.hpp>
#include "emulator.h"

namespace po = boost::program_options;

int main(int argc, const char** argv) {
  po::options_description option_desc{"Options"};

  option_desc.add_options()(
      "trace", po::value<bool>()->default_value(false)->implicit_value(true),
      "enables CPU tracing");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, option_desc), vm);

  po::notify(vm);
  const bool trace = vm["trace"].as<bool>();
  std::string rom_name = argv[1];
  gb::run_with_options(rom_name, trace);

  return 0;
}
