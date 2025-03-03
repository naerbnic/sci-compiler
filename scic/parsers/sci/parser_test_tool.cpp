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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "argparse/argparse.hpp"
#include "scic/parsers/include_context.hpp"
#include "scic/parsers/list_tree/parser.hpp"
#include "scic/parsers/sci/parser.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_readers.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"

namespace parsers::sci {
namespace {

using ::text::TextRange;
using ::tokens::Token;

absl::StatusOr<TextRange> LoadFile(std::filesystem::path const& path) {
  std::ifstream file;
  file.open(path, std::ios::in | std::ios::binary);
  if (!file.good()) {
    return absl::NotFoundError("Could not open file");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return TextRange::WithFilename(util::RefStr(path.string()), buffer.str());
}

absl::StatusOr<std::vector<Token>> TokenizeFile(
    std::filesystem::path const& path) {
  std::ifstream file;
  file.open(path, std::ios::in | std::ios::binary);
  if (!file.good()) {
    return absl::NotFoundError("Could not open file");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return tokens::TokenizeText(
      TextRange::WithFilename(util::RefStr(path.string()), buffer.str()));
}

class ToolIncludeContext : public IncludeContext {
 public:
  ToolIncludeContext(std::vector<std::filesystem::path> include_paths)
      : include_paths_(std::move(include_paths)) {}

  absl::StatusOr<TextRange> LoadTextFromIncludePath(
      std::string_view path) const override {
    std::ifstream file;
    for (auto const& include_path : include_paths_) {
      auto result = LoadFile(include_path / path);
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

  list_tree::Parser parser(&include_context);

  for (auto const& file : files) {
    ASSIGN_OR_RETURN(auto tokens, TokenizeFile(file));
    ASSIGN_OR_RETURN(auto parsed, parser.ParseTree(tokens));
    auto span = absl::MakeConstSpan(parsed);
    auto result = ParseItems(span);
    if (!result.ok()) {
      std::cerr << "Error: " << result.status() << std::endl;
    } else {
      for (auto const& item : result.value()) {
        std::cout << "Parsed: " << item << std::endl;
      }
    }
  }
  return 0;
}

}  // namespace
}  // namespace parsers::sci

int main(int argc, char** argv) {
  auto result = parsers::sci::RunMain(argc, argv);
  if (!result.ok()) {
    std::cerr << "Error: " << result.status() << std::endl;
    return 1;
  }

  return result.value();
}