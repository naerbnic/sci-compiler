#ifndef IO_BUFFER_HPP
#define IO_BUFFER_HPP

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

namespace io {

class ReadBuffer {
 public:
  static std::unique_ptr<ReadBuffer> FromFile(FILE* file);
  static std::unique_ptr<ReadBuffer> FromString(std::string str);

  ReadBuffer() = default;
  ReadBuffer(const ReadBuffer&) = delete;
  virtual ~ReadBuffer() = default;

  virtual int GetLineIndex() const = 0;
  // Returns the current line, including the trailing newline (if any).
  // If not at end, will always have at least one character in the string.
  virtual std::string_view CurrLine() const = 0;
  virtual void AdvanceLine() = 0;
  virtual bool IsAtEnd() const = 0;
};

}  // namespace io

#endif