#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/frontend/flags.hpp"
#include "scic/parsers/list_tree/parser.hpp"
#include "scic/parsers/sci/parser.hpp"
#include "scic/sem/code_builder.hpp"
#include "scic/sem/input.hpp"
#include "scic/sem/module_env.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_readers.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"

namespace frontend {

template <typename T>
std::vector<T> ConcatVectors(std::vector<T> single) {
  return single;
}

template <typename T, typename... Args>
std::vector<T> ConcatVectors(std::vector<T> first, std::vector<T> second,
                             Args... args) {
  std::ranges::move(std::move(second), std::back_inserter(first));
  auto rest = ConcatVectors(std::move(first), std::forward<Args>(args)...);
  return first;
}

absl::StatusOr<text::TextRange> LoadFile(std::filesystem::path path) {
  std::ifstream file;
  file.open(path, std::ios::in | std::ios::binary);
  if (!file.good()) {
    return absl::NotFoundError("Could not open file");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return text::TextRange::WithFilename(util::RefStr(path.string()),
                                       buffer.str());
}

absl::StatusOr<std::vector<tokens::Token>> TokenizeFile(
    std::filesystem::path const& path) {
  ASSIGN_OR_RETURN(auto text, LoadFile(std::move(path)));
  return tokens::TokenizeText(std::move(text));
}

class ToolIncludeContext : public parsers::list_tree::IncludeContext {
 public:
  ToolIncludeContext(std::vector<std::filesystem::path> include_paths)
      : include_paths_(std::move(include_paths)) {}

  absl::StatusOr<std::vector<tokens::Token>> LoadTokensFromInclude(
      std::string_view path) const override {
    std::ifstream file;
    for (auto const& include_path : include_paths_) {
      auto result = TokenizeFile(include_path / path);
      if (result.ok()) {
        return std::move(result).value();
      }

      if (!absl::IsNotFound(result.status())) {
        return result.status();
      }
    }

    return absl::NotFoundError(
        absl::StrFormat("Could not find include file: %s", path));
  }

 private:
  std::vector<std::filesystem::path> include_paths_;
};

absl::Status RunMain(const CompilerFlags& flags) {
  // Load our files into memory.
  ASSIGN_OR_RETURN(auto selector_tokens, TokenizeFile(flags.selector_file));
  ASSIGN_OR_RETURN(auto classdef_tokens, TokenizeFile(flags.classdef_file));
  ASSIGN_OR_RETURN(auto system_header_tokens,
                   TokenizeFile(flags.system_header));
  ASSIGN_OR_RETURN(auto game_header_tokens, TokenizeFile(flags.game_header));

  auto global_tokens = ConcatVectors(
      std::move(selector_tokens), std::move(classdef_tokens),
      std::move(system_header_tokens), std::move(game_header_tokens));

  std::vector<std::vector<tokens::Token>> source_file_tokens;
  for (const auto& file : flags.files) {
    ASSIGN_OR_RETURN(auto source_text, TokenizeFile(file));
    source_file_tokens.push_back(std::move(source_text));
  }

  std::vector<std::filesystem::path> include_paths;
  for (auto include_path_str : flags.include_paths) {
    include_paths.push_back(std::filesystem::path(include_path_str));
  }

  ToolIncludeContext include_context(std::move(include_paths));

  parsers::list_tree::Parser global_parser(&include_context);

  for (auto const& define : flags.command_line_defines) {
    // Tokenize each command-line define and add it to the parser.
    ASSIGN_OR_RETURN(auto tokens,
                     tokens::TokenizeText(text::TextRange::WithFilename(
                         util::RefStr("<command-line>"), define.second)));
    global_parser.AddDefine(define.first, std::move(tokens));
  }

  ASSIGN_OR_RETURN(auto global_list_tree,
                   global_parser.ParseTree(std::move(global_tokens)));

  // Keep the defines from the global parser for the individual files.
  auto global_defines = global_parser.defines();

  // Parse the global trees, and add it to the global AST.
  auto global_items_result = parsers::sci::ParseItems(global_list_tree);

  if (!global_items_result.ok()) {
    std::cerr << global_items_result.status() << std::endl;
    return absl::FailedPreconditionError("Failed to parse global items");
  }

  sem::Input input;

  input.global_items = std::move(global_items_result).value();

  for (auto& source_tokens : source_file_tokens) {
    parsers::list_tree::Parser source_parser(&include_context);
    for (auto const& entry : global_defines) {
      source_parser.AddDefine(entry.first, entry.second);
    }

    ASSIGN_OR_RETURN(auto source_list_tree,
                     source_parser.ParseTree(std::move(source_tokens)));

    auto source_items_result = parsers::sci::ParseItems(source_list_tree);

    if (!source_items_result.ok()) {
      std::cerr << source_items_result.status() << std::endl;
      return absl::FailedPreconditionError("Failed to parse source items");
    }

    input.modules.push_back(sem::Input::Module{
        .module_items = std::move(source_items_result).value(),
    });
  }

  ASSIGN_OR_RETURN(auto compilation_env, sem::BuildCompilationEnvironment(
                                             flags.codegen_options, input));

  // Perform code generation.
  for (auto const* module : compilation_env.module_envs()) {
    RETURN_IF_ERROR(sem::BuildCode(module));
  }

  return absl::OkStatus();
}

}  // namespace frontend

int main(int argc, char** argv) {
  // Initialize Abseil symbolizer, so errors will report correct info.
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  frontend::CompilerFlags flags;
  try {
    flags = frontend::ExtractFlags(argc, argv);
  } catch (const std::exception& err) {
    return 1;
  }

  auto status = frontend::RunMain(flags);
  if (!status.ok()) {
    std::cerr << status.message() << std::endl;
    return 1;
  }
  return 0;
}