#include "scic/compiler.hpp"

#include <memory>

#include "scic/alist.hpp"

Compiler::Compiler() {
  hunkList = std::make_unique<CodeList>();
  heapList = std::make_unique<FixupList>();
}

Compiler::~Compiler() = default;