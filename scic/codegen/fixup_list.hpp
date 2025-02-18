#ifndef FIXUP_LIST_HPP
#define FIXUP_LIST_HPP

#include <cstddef>
#include <memory>

#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/listing.hpp"

// A pure-virtual class that gives context if a node is located in the
// heap.
class HeapContext {
 public:
  virtual ~HeapContext() = default;

  virtual bool IsInHeap(ANode const* node) const = 0;
};

class FixupList {
  // A FixupList is an AList which has elements in it which need to be relocated
  // by the interpreter at load time.  It builds a table of offsets needing
  // relocation which is appended to the end of the object code being generated.
 public:
  FixupList();
  ~FixupList();

  void list(ListingFile* listFile);
  void emit(HeapContext*, OutputFile*);
  size_t setOffset(size_t ofs);

  /**
   * @brief Adds a fixup to the list.
   *
   * This method adds a fixup to the internal list, associating a node with a
   * relative offset.
   *
   * @param node A pointer to the ANode object that represents the node to be
   * fixed up.
   * @param rel_ofs The relative offset where the fixup should be applied.
   */
  void addFixup(ANode const* node, std::size_t rel_ofs);

  bool contains(ANode const* ln) { return root_->contains(ln); }

  ANodeList* getBody() { return bodyList_; }

  ANode* getRoot() { return root_.get(); }

 protected:
  struct Offset {
    ANode const* node_base;
    std::size_t rel_offset;

    std::size_t offset() const {
      if (!node_base) return rel_offset;
      return *node_base->offset + rel_offset;
    }
  };
  std::unique_ptr<ANode> root_;
  ANodeList* bodyList_;
  ANodeList* fixupList_;
};

#endif