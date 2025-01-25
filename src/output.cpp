//	output.cpp	sc
//		write binary output files

#include "output.hpp"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "error.hpp"
#include "jeff.hpp"
#include "memtype.hpp"
#include "platform.hpp"
#include "resource.hpp"
#include "sc.hpp"
#include "sol.hpp"

bool highByteFirst;

OutputFile::OutputFile(std::string fileName) : fileName(fileName) {
  fp = CreateOutputFile(fileName);
  if (!fp) Panic("Can't open output file %s", fileName);
}

OutputFile::~OutputFile() { fclose(fp); }

void OutputFile::SeekTo(long offset) { fseek(fp, offset, SEEK_SET); }

int OutputFile::WriteNullTerminatedString(std::string_view str) {
  Write(str.data(), str.length());
  WriteByte(0);
  return str.length() + 1;
}

int OutputFile::Write(const char* str) {
  int length = strlen(str);
  WriteWord(length);
  Write(str, length);
  return length + sizeof(SCIWord);
}

void OutputFile::WriteWord(int16_t w) {
  // write a word in proper byte order
  //	NOTE:  this code assumes knowledge of the size of SCIWord

  if (highByteFirst)
    // swap high and low bytes
    w = ((w >> 8) & 0xFF) | (w << 8);

  Write(&w, sizeof w);
}

void OutputFile::WriteByte(ubyte b) { Write(&b, sizeof b); }

void OutputFile::Write(const void* mp, size_t len) {
  if (fwrite(mp, 1, len, fp) != static_cast<ssize_t>(len))
    Panic("Error writing %s", fileName);
}

//////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<OutputFile> OpenObjFile(MemType type, std::string name);
static std::string MakeObjFileName(MemType type);

ObjFiles OpenObjFiles() {
  //	open the new files
  return ObjFiles{
      .heap = OpenObjFile(MemResHeap, MakeObjFileName(MemResHeap)),
      .hunk = OpenObjFile(MemResHunk, MakeObjFileName(MemResHunk)),
  };
}

static std::string MakeObjFileName(MemType type) {
  std::string resName = ResNameMake(type, script);
  std::string dest = MakeName(outDir, resName, resName);
  DeletePath(dest);
  return dest;
}

static std::unique_ptr<OutputFile> OpenObjFile(MemType type, std::string name) {
  auto out = std::make_unique<OutputFile>(std::move(name));

  // Put out the header information.
  ubyte header[2];
  header[0] = (char)type;
  header[1] = 0;
  out->Write(header, sizeof header);

  return out;
}
