//	output.hpp
//		write binary output files

#ifndef OUTPUT_HPP
#define OUTPUT_HPP

#include <cstddef>
#include <cstdint>

class OutputFile {
 public:
  OutputFile(const char* fileName);
  ~OutputFile();

  void SeekTo(long offset);
  void WriteByte(uint8_t);
  void WriteOp(uint8_t op) { WriteByte(op); }
  void WriteWord(int16_t);
  void Write(const void*, size_t);
  int Write(const char*);

 protected:
  int fd;
  const char* fileName;
};

void OpenObjFiles(OutputFile** heapOut, OutputFile** hunkOut);

extern bool highByteFirst;

#endif
