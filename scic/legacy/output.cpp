//	output.cpp	sc
//		write binary output files

#include "scic/legacy/output.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/codegen/common.hpp"
#include "scic/legacy/config.hpp"
#include "scic/legacy/memtype.hpp"
#include "scic/legacy/resource.hpp"
#include "util/platform/platform.hpp"

using ::codegen::SCIWord;

OutputFile::OutputFile(std::string fileName) : fileName(fileName) {
  fp = CreateOutputFile(fileName);
  if (!fp)
    throw std::runtime_error(
        absl::StrFormat("Can't open output file %s", fileName));
}

OutputFile::~OutputFile() { fclose(fp); }

void OutputFile::SeekTo(long offset) { fseek(fp, offset, SEEK_SET); }

int OutputFile::WriteNullTerminatedString(std::string_view str) {
  Write(str.data(), str.length());
  WriteByte(0);
  return str.length() + 1;
}

int OutputFile::Write(std::string_view str) {
  int length = str.length();
  WriteWord(length);
  Write(str.data(), length);
  return length + sizeof(SCIWord);
}

void OutputFile::WriteWord(int16_t w) {
  // write a word in proper byte order
  //	NOTE:  this code assumes knowledge of the size of SCIWord

  // Ensure we're working with the raw bits by using an unsigned value.
  uint16_t u = w;

  std::endian targetEndian =
      gConfig->highByteFirst ? std::endian::big : std::endian::little;

  if (std::endian::native != targetEndian) {
    // Swap the bytes.
    u = (u >> 8) | (u << 8);
  }

  Write(&u, sizeof u);
}

void OutputFile::WriteByte(uint8_t b) { Write(&b, sizeof b); }

void OutputFile::Write(const void* mp, size_t len) {
  if (fwrite(mp, 1, len, fp) != static_cast<std::size_t>(len))
    throw std::runtime_error(absl::StrFormat("Error writing %s", fileName));
}

//////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<OutputFile> OpenObjFile(std::string name);
static std::string MakeObjFileName(MemType type, int scriptNum);

ObjFiles OpenObjFiles(int scriptNum) {
  //	open the new files
  return ObjFiles(OpenObjFile(MakeObjFileName(MemResHeap, scriptNum)),
                  OpenObjFile(MakeObjFileName(MemResHunk, scriptNum)));
}

static std::string MakeObjFileName(MemType type, int scriptNum) {
  std::string resName = ResNameMake(type, scriptNum);
  std::string dest = (gConfig->outDir / resName).string();
  DeletePath(dest);
  return dest;
}

static std::unique_ptr<OutputFile> OpenObjFile(std::string name) {
  return std::make_unique<OutputFile>(std::move(name));
}
