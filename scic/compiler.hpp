#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <memory>

class FixupList;
struct CodeList;

struct Compiler {
  Compiler();

  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<CodeList> hunkList;
};

extern std::unique_ptr<Compiler> gSc;

#endif