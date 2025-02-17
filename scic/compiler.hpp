#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstddef>
#include <memory>

#include "scic/alist.hpp"

class FixupList;
struct CodeList;

struct Compiler : public FixupContext {
  Compiler();
  ~Compiler();

  bool HeapHasNode(ANode* node) const override;
  void AddFixup(std::size_t ofs) override;

  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<CodeList> hunkList;
};

#endif