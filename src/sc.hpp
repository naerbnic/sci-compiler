//	sc.hpp
//		standard header for all SC files

#ifndef SC_HPP
#define SC_HPP

#include <filesystem>
#include <memory>

#define REQUIRED 1
#define OPTIONAL 0

#define UNDEFINED 0
#define DEFINED 1

class FixupList;
struct CodeList;

struct Compiler {
  Compiler();

  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<CodeList> hunkList;
};

extern std::unique_ptr<Compiler> gSc;

extern int gScript;

#endif
