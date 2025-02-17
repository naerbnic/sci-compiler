#ifndef FIXUP_LIST_HPP
#define FIXUP_LIST_HPP

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "scic/alist.hpp"
#include "scic/casts.hpp"
#include "scic/listing.hpp"

class FixupList {
  // A FixupList is an AList which has elements in it which need to be relocated
  // by the interpreter at load time.  It builds a table of offsets needing
  // relocation which is appended to the end of the object code being generated.
 public:
  FixupList();
  ~FixupList();

  void clear();

  void list(ListingFile* listFile);
  void emit(FixupContext*, OutputFile*);
  size_t setOffset(size_t ofs);

  // Increment the number of elements needing fixup.  This is
  // called each time we generate an ANode requiring fixup.

  void initFixups();
  // Called once the module has been compiled to allocate space
  // for the fixup table based on 'numFixups'.

  void listFixups(ListingFile* listFile);
  // List the fixup table.

  void emitFixups(OutputFile*);
  // Emit the fixup table to the object file.

  void addFixup(size_t ofs);
  // The word at offset 'ofs' in the object file needs relocation.
  // Add the offset to the fixup table.

  ANode* front() { return down_cast<ANode>(list_.list_.frontPtr()); }

  void addAfter(ANode* ln, std::unique_ptr<ANode> nn) {
    list_.list_.findIter(ln).addAfter(std::move(nn));
  }

  bool contains(ANode* ln) { return list_.list_.contains(ln); }

  template <class T, class... Args>
  T* newNode(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    auto* node_ptr = node.get();
    list_.list_.addBack(std::move(node));
    return node_ptr;
  }

  AList* getList() { return &list_; }

 protected:
  AList list_;
  std::vector<size_t> fixups;  // storage for fixup values
  size_t fixOfs;               // offset of start of fixups
};

struct CodeList : FixupList {
  // The CodeList class specializes the FixupList class for actual p-code,
  // which is the only sort of AList currently optimized.

  void optimize();
};

#endif