#include "scic/frontend/flags.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "argparse/argparse.hpp"
#include "scic/codegen/code_generator.hpp"

namespace frontend {

namespace {

inline constexpr std::string_view kProgramBanner =
    "SCI Script Compiler 5.0 (" __DATE__ " " __TIME__
    ")\n"
    "(c) 2024 by Brian Chin and Digital Alchemy Studios, LLC. Released under "
    "the MIT License.";
}

CompilerFlags ExtractFlags(int argc, char** argv) {
  CompilerFlags flags;

  auto InstallCommandLineDefine = [&flags](std::string define) {
    auto pos = define.find('=');
    if (pos == std::string::npos) {
      flags.command_line_defines[define] = "";
    } else {
      flags.command_line_defines[define.substr(0, pos)] =
          define.substr(pos + 1);
    }
  };
  argparse::ArgumentParser program("sc", std::string(frontend::kProgramBanner));
  program.add_argument("-a")
      .help("abort compile if database locked")
      .default_value(false)
      .flag();
  program.add_argument("-d")
      .help("include debug info")
      .default_value(false)
      .flag();
  program.add_argument("-D")
      .help("command line define (e.g. -DMAC or -DMAC=1)")
      .action(InstallCommandLineDefine)
      .append()
      .nargs(1);
  program.add_argument("-g")
      .help("maximum number of global or local variables")
      .default_value(750ul);
  program.add_argument("-l")
      .help("generate a code listing")
      .default_value(false)
      .flag();
  program.add_argument("-n")
      .help("no auto-naming of objects")
      .default_value(false)
      .flag();
  program.add_argument("-o").help("set output directory").default_value("");
  program.add_argument("-O")
      .help("output the 'offsets' file")
      .default_value(false)
      .flag();
  program.add_argument("-s")
      .help("show forward-referenced selectors")
      .default_value(false)
      .flag();
  program.add_argument("-u")
      .help("don't lock class database")
      .default_value(false)
      .flag();
  program.add_argument("-v").help("verbose output").default_value(false).flag();
  program.add_argument("-w")
      .help("output words high-byte first (for M68000)")
      .default_value(false)
      .flag();
  program.add_argument("-z")
      .help("turn off optimization")
      .default_value(false)
      .flag();
  program.add_argument("-t")
      .help("Set the target architecture. Valid values are: SCI_1_1, SCI_2")
      .default_value(std::string{"SCI_2"});
  program.add_argument("--selector_file")
      .help("The selector file to use during compilation")
      .default_value(std::string{"selector"});
  program.add_argument("--classdef_file")
      .help("The class definition file to use during compilation")
      .default_value(std::string{"classdef"});
  program.add_argument("--system_header")
      .help("The system header file to use during compilation")
      .default_value(std::string{"system.sh"});
  program.add_argument("--game_header")
      .help("The game header file to use during compilation")
      .default_value(std::string{"game.sh"});
  program.add_argument("-I", "--include_path")
      .help("List of directories to use for include files")
      .default_value(std::vector<std::string>())
      .append()
      .nargs(1);
  program.add_argument("files")
      .default_value(std::vector<std::string>())
      .remaining();

  try {
    program.parse_args(argc, argv);
    auto codegen_optimization = program.get<bool>("-z")
                                    ? codegen::Optimization::NO_OPTIMIZE
                                    : codegen::Optimization::OPTIMIZE;
    auto target_name = program.get<std::string>("-t");
    codegen::SciTarget target;
    if (target_name == "SCI_1_1") {
      target = codegen::SciTarget::SCI_1_1;
    } else if (target_name == "SCI_2") {
      target = codegen::SciTarget::SCI_2;
    } else {
      throw std::runtime_error("Invalid target architecture");
    }
    program.parse_args(argc, argv);
    flags.abort_if_locked = program.get<bool>("-a");
    flags.include_debug_info = program.get<bool>("-d");
    flags.generate_code_listing = program.get<bool>("-l");
    flags.output_directory = program.get<std::string>("-o");
    flags.verbose_output = program.get<bool>("-v");
    flags.output_words_high_byte_first = program.get<bool>("-w");
    flags.codegen_options = codegen::CodeGenerator::Options{
        .target = target,
        .opt = codegen_optimization,
    };
    flags.selector_file = program.get<std::string>("--selector_file");
    flags.classdef_file = program.get<std::string>("--classdef_file");
    flags.system_header = program.get<std::string>("--system_header");
    flags.game_header = program.get<std::string>("--game_header");
    flags.include_paths =
        program.get<std::vector<std::string>>("--include_path");
    flags.files = program.get<std::vector<std::string>>("files");
    return flags;
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    throw;
  }
}

}  // namespace frontend