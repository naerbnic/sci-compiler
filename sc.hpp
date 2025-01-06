//	sc.hpp
//		standard header for all SC files

#ifndef SC_HPP
#define SC_HPP

#include <unistd.h>

#include <cstdint>

// Modes for opening files.
#ifdef _WIN32
#define OMODE (int)(O_BINARY | O_CREAT | O_RDWR | O_TRUNC)
#else
#define OMODE (int)(O_CREAT | O_RDWR | O_TRUNC)
#endif
#define PMODE (int)(S_IREAD | S_IWRITE)

#define REQUIRED 1
#define OPTIONAL 0

#define UNDEFINED 0
#define DEFINED 1

typedef const char* strptr;
typedef unsigned char ubyte;
typedef unsigned short uword;

class FixupList;
class CodeList;

struct Compiler {
  Compiler();
  ~Compiler();

  FixupList* heapList;
  CodeList* hunkList;
} extern* sc;

extern bool includeDebugInfo;
extern char outDir[];
extern char progName[];
extern int script;
extern bool verbose;

#endif
