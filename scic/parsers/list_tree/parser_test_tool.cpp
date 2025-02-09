#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "argparse/argparse.hpp"
#include "scic/parsers/list_tree/parser.hpp"
#include "scic/tokenizer/text_contents.hpp"
#include "scic/tokenizer/token.hpp"
#include "scic/tokenizer/token_readers.hpp"
#include "util/status/status_macros.hpp"

namespace {

using ::tokenizer::TextRange;
using ::tokenizer::Token;

absl::StatusOr<std::vector<Token>> TokenizeFile(
    std::filesystem::path const& path) {
  std::ifstream file;
  file.open(path, std::ios::in | std::ios::binary);
  if (!file.good()) {
    return absl::NotFoundError("Could not open file");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return tokenizer::TokenizeText(
      TextRange::WithFilename(path.string(), buffer.str()));
}

class ToolIncludeContext : public parser::list_tree::IncludeContext {
 public:
  ToolIncludeContext(std::vector<std::filesystem::path> include_paths)
      : include_paths_(std::move(include_paths)) {}

  absl::StatusOr<std::vector<Token>> LoadTokensFromInclude(
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

absl::StatusOr<int> RunMain(int argc, char** argv) {
  argparse::ArgumentParser program("parser_test_tool", "Test tool for parser");
  program.add_argument("-I", "--include")
      .help("Add an include path")
      .default_value(std::vector<std::string>())
      .nargs(1);
  program.add_argument("files")
      .default_value(std::vector<std::string>())
      .remaining();

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  std::vector<std::filesystem::path> include_paths;

  for (auto include_path_str : program.get<std::vector<std::string>>("-I")) {
    include_paths.push_back(std::filesystem::path(include_path_str));
  }

  auto files = program.get<std::vector<std::string>>("files");

  ToolIncludeContext include_context(std::move(include_paths));

  for (auto const& file : files) {
    ASSIGN_OR_RETURN(auto tokens, TokenizeFile(file));
    ASSIGN_OR_RETURN(auto parsed, parser::list_tree::ParseListTree(
                                      tokens, &include_context, {}));

    // Print the parsed tree.
    absl::PrintF("Parsed %s:\n", file);
    for (auto const& expr : parsed) {
      absl::PrintF("  %v\n", expr);
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  auto result = RunMain(argc, argv);
  if (!result.ok()) {
    std::cerr << "Error: " << result.status() << std::endl;
    return 1;
  }

  return result.value();
}