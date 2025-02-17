#include "scic/fixup_list.hpp"

#include <cstddef>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/common.hpp"
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

class ANComputedWord : public ANode {
 public:
  size_t size() override { return 2; }
  void list(ListingFile* listFile) override {
    listFile->ListWord(*offset, value());
  }
  void emit(OutputFile* out) override { out->WriteWord(value()); }

 protected:
  virtual SCIWord value() const = 0;
};

class ANOffsetWord : public ANComputedWord {
 public:
  ANOffsetWord(ANode* target, std::size_t rel_offset)
      : target(target), rel_offset(rel_offset) {}

  ANode* target;
  std::size_t rel_offset;

 protected:
  SCIWord value() const override { return *target->offset + rel_offset; }
};

class ANCountWord : public ANComputedWord {
 public:
  ANCountWord(AList* target) : target(target) {}

  AList* target;

 protected:
  SCIWord value() const override { return target->length(); }
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
  auto* fixupCountWord = fixupTableBlock->entries.newNode<ANCountWord>(nullptr);
  fixupTable_ = fixupTableBlock->entries.newNode<ANTable>("fixup table");
  fixupCountWord->target = &fixupTable_->entries;
  fixupOffsetNode->target = fixupTableBlock;
}

FixupList::~FixupList() {}

size_t FixupList::setOffset(size_t ofs) { return list_.setOffset(ofs); }

void FixupList::initFixups() {}

void FixupList::listFixups(ListingFile* listFile) { list_.list(listFile); }

void FixupList::addFixup(ANode* node, std::size_t rel_ofs) {
  fixupTable_->entries.newNode<ANOffsetWord>(node, rel_ofs);
}

void FixupList::list(ListingFile* listFile) { list_.list(listFile); }

void FixupList::emit(HeapContext* heap_ctxt, OutputFile* out) {
  initFixups();
  {
    FixupListContext fixup_ctxt(this, heap_ctxt);
    list_.collectFixups(&fixup_ctxt);
    fixupTable_->entries.setOffset(*fixupTable_->offset);
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
  gShrink = true;
  setOffset(0);

  // Continue resolving and converting to byte offsets until we've shrunk
  // the code as far as it will go.
  size_t curLen = 0, oldLen;
  do {
    oldLen = curLen;
    curLen = setOffset(0);
  } while (oldLen > curLen);

  // Now stabilize the code and offsets by resolving without allowing
  // conversion to byte offsets.
  gShrink = false;
  do {
    oldLen = curLen;
    curLen = setOffset(0);
  } while (oldLen != curLen);
}