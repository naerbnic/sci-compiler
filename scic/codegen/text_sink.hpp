#ifndef CODEGEN_TEXT_SINK_HPP
#define CODEGEN_TEXT_SINK_HPP

#include <filesystem>
#include <memory>
#include <string_view>

namespace codegen {

class TextSink {
 public:
  static std::unique_ptr<TextSink> FileTrunc(std::filesystem::path file_name);
  static std::unique_ptr<TextSink> Null();
  virtual ~TextSink() = default;

  virtual bool Write(std::string_view text) = 0;
};

}  // namespace codegen

#endif