#ifndef FIXUP_LIST_HPP
#define FIXUP_LIST_HPP

#include <cstddef>
#include <memory>
#include <utility>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/anode_impls.hpp"
#include "scic/listing.hpp"

// A pure-virtual class that gives context if a node is located in the
// heap.
class HeapContext {
 public:
  virtual ~HeapContext() = default;

  virtual bool IsInHeap(ANode* node) const = 0;
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
  void addFixup(ANode* node, std::size_t rel_ofs);

  bool contains(ANode* ln) {
    for (auto& entry : list_) {
      if (entry.contains(ln)) return true;
    }
    return false;
  }

 private:
  template <class T, class... Args>
  T* newNode(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    auto* node_ptr = node.get();
    list_.list_.addBack(std::move(node));
    return node_ptr;
  }

 public:
  ANodeList* getList() { return bodyTable_->getList(); }

 protected:
  struct Offset {
    ANode* node_base;
    std::size_t rel_offset;

    std::size_t offset() const {
      if (!node_base) return rel_offset;
      return *node_base->offset + rel_offset;
    }
  };
  ANodeList list_;
  ANTable* bodyTable_;
  ANTable* fixupTable_;
};

#endif