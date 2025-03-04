//	output.hpp
//		write binary output files

#ifndef OUTPUT_HPP
#define OUTPUT_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "scic/codegen/output.hpp"

class OutputFile : public codegen::OutputWriter {
 public:
  OutputFile(std::string fileName);
  ~OutputFile();

  void SeekTo(long offset) override;
  void WriteByte(uint8_t) override;
  void WriteOp(uint8_t op) override { WriteByte(op); }
  void WriteWord(int16_t) override;
  void Write(const void*, size_t) override;
  int WriteNullTerminatedString(std::string_view str) override;
  int Write(std::string_view) override;

 protected:
  FILE* fp;
  std::string fileName;
};

class ObjFiles : public codegen::OutputFiles {
 public:
  ObjFiles(std::unique_ptr<OutputFile> heap, std::unique_ptr<OutputFile> hunk)
      : heap(std::move(heap)), hunk(std::move(hunk)) {}

  virtual codegen::OutputWriter* GetHeap() override { return heap.get(); };
  virtual codegen::OutputWriter* GetHunk() override { return hunk.get(); }

 private:
  std::unique_ptr<OutputFile> heap;
  std::unique_ptr<OutputFile> hunk;
};

ObjFiles OpenObjFiles(int scriptNum);

#endif
