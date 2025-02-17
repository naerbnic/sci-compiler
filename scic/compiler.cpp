#include "scic/compiler.hpp"

#include <memory>

#include "scic/alist.hpp"

std::unique_ptr<Compiler> gSc;

Compiler::Compiler() {
  hunkList = std::make_unique<CodeList>();
  heapList = std::make_unique<FixupList>();
}