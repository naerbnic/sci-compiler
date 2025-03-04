#include "scic/codegen/text_sink.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "absl/strings/str_format.h"

namespace codegen {
namespace {

class FileTextSink : public TextSink {
 public:
  explicit FileTextSink(std::ofstream file) : file_(std::move(file)) {}

  bool Write(std::string_view text) override {
    file_ << text;
    return file_.good();
  }

 private:
  std::ofstream file_;
};

class NullTextSink : public TextSink {
 public:
  bool Write(std::string_view) override { return true; }
};

}  // namespace

std::unique_ptr<TextSink> TextSink::FileTrunc(std::filesystem::path file_name) {
  std::ofstream file;
  file.open(file_name, std::ios::out | std::ios::trunc);
  if (!file.good()) {
    throw std::runtime_error(
        absl::StrFormat("Could not open file \"%s\" for writing", file_name));
  }
  return std::make_unique<FileTextSink>(std::move(file));
}

std::unique_ptr<TextSink> TextSink::Null() {
  return std::make_unique<NullTextSink>();
}

}  // namespace codegen