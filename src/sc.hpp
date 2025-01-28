//	sc.hpp
//		standard header for all SC files

#ifndef SC_HPP
#define SC_HPP

#include <memory>
#include <string>

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

typedef unsigned char ubyte;
typedef unsigned short uword;

class FixupList;
struct CodeList;

struct Compiler {
  Compiler();

  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<CodeList> hunkList;
};

extern std::unique_ptr<Compiler> sc;

// The target SCI architecture for the scripts.
enum class SciTargetArch {
  SCI_1_1,
  SCI_2,
};

extern bool includeDebugInfo;
extern std::string outDir;
extern int script;
extern bool verbose;
extern SciTargetArch targetArch;

#endif
