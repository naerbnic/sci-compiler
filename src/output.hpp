//	output.hpp
//		write binary output files

#ifndef OUTPUT_HPP
#define OUTPUT_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

class OutputFile {
 public:
  OutputFile(std::string fileName);
  ~OutputFile();

  void SeekTo(long offset);
  void WriteByte(uint8_t);
  void WriteOp(uint8_t op) { WriteByte(op); }
  void WriteWord(int16_t);
  void Write(const void*, size_t);
  int WriteNullTerminatedString(std::string_view str);
  int Write(std::string_view);

 protected:
  FILE* fp;
  std::string fileName;
};

struct ObjFiles {
  std::unique_ptr<OutputFile> heap;
  std::unique_ptr<OutputFile> hunk;
};

ObjFiles OpenObjFiles();

#endif
