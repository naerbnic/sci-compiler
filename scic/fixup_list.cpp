#include "scic/fixup_list.hpp"

#include <cstddef>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/config.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"

namespace {
class FixupListContext : public FixupContext {
 public:
  FixupListContext(FixupList* fixupList, HeapContext const* heapContext)
      : fixupList_(fixupList), heapContext_(heapContext) {}

  bool HeapHasNode(ANode* node) const override {
    return heapContext_->IsInHeap(node);
  }

  void AddRelFixup(ANode* node, std::size_t ofs) override {
    fixupList_->addFixup(node, ofs);
  }

 private:
  FixupList* fixupList_;
  HeapContext const* heapContext_;
};

class ANWordPadding : public ANode {
 public:
  size_t setOffset(size_t ofs) override {
    offset = ofs;
    if (ofs & 1) {
      ofs++;
    }
    return ofs;
  }
  void list(ListingFile* listFile) override {
    if (*offset & 1) {
      listFile->ListByte(*offset, 0);
    }
  }
  void emit(OutputFile* out) override {
    if (*offset & 1) {
      out->WriteByte(0);
    }
  }
  // Emits the object code for the node to the output file.
};

}  // namespace

///////////////////////////////////////////////////
// Class FixupList
///////////////////////////////////////////////////

FixupList::FixupList() {
  auto* fixupOffsetNode = list_.newNode<ANOffsetWord>(nullptr, 0);
  bodyTable_ = list_.newNode<ANTable>("object file body");
  // We need padding before the fixup table to ensure it is word-aligned.
  list_.newNode<ANWordPadding>();
  // We create a small node graph here, to keep all of the pieces of the
  // fixup table together.
  // The whole fixup table, including the initial count word.
  auto* fixupTableBlock = list_.newNode<ANTable>("fixup table block");
  auto* fixupCountWord =
      fixupTableBlock->getList()->newNode<ANCountWord>(nullptr);
  fixupTable_ = fixupTableBlock->getList()->newNode<ANTable>("fixup table");
  fixupCountWord->target = fixupTable_->getList();
  fixupOffsetNode->target = fixupTableBlock;
}

FixupList::~FixupList() {}

size_t FixupList::setOffset(size_t ofs) { return list_.setOffset(ofs); }

void FixupList::initFixups() {}

void FixupList::listFixups(ListingFile* listFile) { list_.list(listFile); }

void FixupList::addFixup(ANode* node, std::size_t rel_ofs) {
  fixupTable_->getList()->newNode<ANOffsetWord>(node, rel_ofs);
}

void FixupList::list(ListingFile* listFile) { list_.list(listFile); }

void FixupList::emit(HeapContext* heap_ctxt, OutputFile* out) {
  initFixups();
  {
    FixupListContext fixup_ctxt(this, heap_ctxt);
    list_.collectFixups(&fixup_ctxt);
    fixupTable_->getList()->setOffset(*fixupTable_->offset);
  }
  list_.emit(out);
}

///////////////////////////////////////////////////
// Class CodeList
///////////////////////////////////////////////////

void CodeList::optimize() {
  if (!gConfig->noOptimize) {
    for (auto it = list_.iter(); it; ++it)
      while (it->optimize())
        ;
  }

  // Make a first pass, resolving offsets and converting to byte offsets
  // where possible.
  setOffset(0);

  // Continue resolving and converting to byte offsets until we've shrunk
  // the code as far as it will go.
  while (true) {
    bool changed = false;
    for (auto it = list_.iter(); it; ++it) {
      changed |= it->tryShrink();
    }
    if (!changed) break;

    setOffset(0);
  }
}