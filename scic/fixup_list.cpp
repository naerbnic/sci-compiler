#include "scic/fixup_list.hpp"

#include <cstddef>
#include <memory>
#include <utility>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/anode_impls.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"

namespace {
class FixupListContext : public FixupContext {
 public:
  FixupListContext(FixupList* fixupList, HeapContext const* heapContext)
      : fixupList_(fixupList), heapContext_(heapContext) {}

  bool HeapHasNode(ANode const* node) const override {
    return heapContext_->IsInHeap(node);
  }

  void AddRelFixup(ANode const* node, std::size_t ofs) override {
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
  void list(ListingFile* listFile) const override {
    if (*offset & 1) {
      listFile->ListByte(*offset, 0);
    }
  }
  void emit(OutputFile* out) const override {
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
  auto root = std::make_unique<ANComposite<ANode>>();
  auto* list = root->getList();
  root_ = std::move(root);
  auto* fixupOffsetNode = list->newNode<ANOffsetWord>(nullptr, 0);
  bodyList_ = list->newNode<ANTable>("object file body")->getList();
  // We need padding before the fixup table to ensure it is word-aligned.
  list->newNode<ANWordPadding>();
  // We create a small node graph here, to keep all of the pieces of the
  // fixup table together.
  // The whole fixup table, including the initial count word.
  auto* fixupTableBlock = list->newNode<ANTable>("fixup table block");
  auto* fixupCountWord =
      fixupTableBlock->getList()->newNode<ANCountWord>(nullptr);
  fixupList_ =
      fixupTableBlock->getList()->newNode<ANTable>("fixup table")->getList();
  fixupCountWord->target = fixupList_;
  fixupOffsetNode->target = fixupTableBlock;
}

FixupList::~FixupList() {}

size_t FixupList::setOffset(size_t ofs) { return root_->setOffset(ofs); }

void FixupList::addFixup(ANode const* node, std::size_t rel_ofs) {
  fixupList_->newNode<ANOffsetWord>(node, rel_ofs);
}

void FixupList::list(ListingFile* listFile) { root_->list(listFile); }

void FixupList::emit(HeapContext* heap_ctxt, OutputFile* out) {
  {
    FixupListContext fixup_ctxt(this, heap_ctxt);
    root_->collectFixups(&fixup_ctxt);
    root_->setOffset(0);
  }
  root_->emit(out);
}