#include "scic/compiler.hpp"

#include <cstddef>
#include <memory>

#include "scic/alist.hpp"

Compiler::Compiler() {
  hunkList = std::make_unique<CodeList>();
  heapList = std::make_unique<FixupList>();
}

Compiler::~Compiler() = default;

bool Compiler::HeapHasNode(ANode* node) const {
  return heapList->contains(node);
}

void Compiler::AddFixup(std::size_t ofs) { heapList->addFixup(ofs); }