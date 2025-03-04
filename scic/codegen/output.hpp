#ifndef CODEGEN_OUTPUT_HPP
#define CODEGEN_OUTPUT_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace codegen {

class OutputWriter {
 public:
  virtual ~OutputWriter() = default;
  virtual void WriteByte(std::uint8_t) = 0;
  virtual void WriteOp(std::uint8_t op) = 0;
  virtual void WriteWord(std::int16_t) = 0;
  virtual void Write(const void*, std::size_t) = 0;
  virtual int WriteNullTerminatedString(std::string_view str) = 0;
  virtual int Write(std::string_view) = 0;
};

class OutputFiles {
 public:
  virtual ~OutputFiles() = default;

  virtual OutputWriter* GetHeap() = 0;
  virtual OutputWriter* GetHunk() = 0;
};

}  // namespace codegen
#endif