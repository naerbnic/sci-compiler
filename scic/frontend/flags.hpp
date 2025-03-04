#ifndef FRONTEND_FLAGS_HPP
#define FRONTEND_FLAGS_HPP

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "scic/codegen/code_generator.hpp"
namespace frontend {

class CompilerFlags {
 public:
  bool abort_if_locked = false;
  bool include_debug_info = false;
  std::map<std::string, std::string> command_line_defines;
  bool generate_code_listing = false;
  std::filesystem::path output_directory;
  bool verbose_output = false;
  bool output_words_high_byte_first = false;
  codegen::CodeGenerator::Options codegen_options;
  std::string selector_file = "selector";
  std::string classdef_file = "classdef";
  std::string system_header = "system.sh";
  std::string game_header = "game.sh";
  std::vector<std::string> include_paths;
  std::vector<std::string> files;
};

CompilerFlags ExtractFlags(int argc, char** argv);

}  // namespace frontend
#endif