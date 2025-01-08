//	output.cpp	sc
//		write binary output files

#include "output.hpp"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.hpp"
#include "jeff.hpp"
#include "memtype.hpp"
#include "resource.hpp"
#include "sc.hpp"
#include "sol.hpp"

bool highByteFirst;

OutputFile::OutputFile(const char* fileName) : fileName(fileName) {
  if ((fd = open(fileName, OMODE, PMODE)) == -1)
    Panic("Can't open output file %s", fileName);
}

OutputFile::~OutputFile() { close(fd); }

void OutputFile::SeekTo(long offset) { lseek(fd, offset, 0); }

int OutputFile::Write(std::string_view str) {
  int length = str.length();
  WriteWord(length);
  Write(str.data(), length);
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
  if (write(fd, mp, len) != static_cast<ssize_t>(len))
    Panic("Error writing %s", fileName);
}

//////////////////////////////////////////////////////////////////////////////

static OutputFile* OpenObjFile(MemType type, const char* name);
static void MakeObjFileName(char* dest, MemType type);

void OpenObjFiles(OutputFile** heapOut, OutputFile** hunkOut) {
  char heapName[_MAX_PATH + 1];
  char hunkName[_MAX_PATH + 1];

  //	make the names and delete old objects
  MakeObjFileName(heapName, MemResHeap);
  MakeObjFileName(hunkName, MemResHunk);

  //	open the new files
  *heapOut = OpenObjFile(MemResHeap, heapName);
  *hunkOut = OpenObjFile(MemResHunk, hunkName);
}

static void MakeObjFileName(char* dest, MemType type) {
  const char* resName = ResNameMake(type, script);
  MakeName(dest, outDir, resName, resName);
  unlink(dest);
}

static OutputFile* OpenObjFile(MemType type, const char* name) {
  OutputFile* out = new OutputFile(name);

  // Put out the header information.
  ubyte header[2];
  header[0] = (char)type;
  header[1] = 0;
  out->Write(header, sizeof header);

  return out;
}
