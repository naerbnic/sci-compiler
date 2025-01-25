//	alist.hpp
// 	definitions for assembly

#ifndef ALIST_HPP
#define ALIST_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "list.hpp"

struct ANOpCode;
struct ANode;
class OutputFile;
struct AList;

class AListIter {
  // AListIter is an iterator for the AList class.
 public:
  ANode* get();
  void advance();
  explicit operator bool();
  ANode* operator->();
  std::unique_ptr<ANode> remove(ANode* an);
  ANode* replaceWith(ANode* an, std::unique_ptr<ANode> nn);

  // Return a pointer to the next opcode node if it is opcode
  // 'op', NULL if it isn't.
  ANOpCode* findOp(uint32_t op);

  bool removeOp(uint32_t op);
  // If next opcode in the list is 'op', remove it.

 private:
  friend struct AList;
  AListIter(ListIter iter) : iter_(std::move(iter)) {}
  ListIter iter_;
};

struct AList : List {
  // AList (assembly list) is a list of ANodes (assembly nodes).

  ANOpCode* nextOp(ANode* start);
  // Return a pointer to the next opcode node after 'start' in
  // the list or NULL if there are none.

  size_t size();
  // Return the number of bytes of code generated by the opcodes
  // in this list.

  void list();
  // List a header for this AList, then invoke the list() methods
  // of each element in the list.

  void emit(OutputFile*);
  // Invoke the emit() methods of each element in the list.

  size_t setOffset(size_t ofs);
  // Set the offsets of each element of the list based on the
  // start of the list being at offset 'ofs'.

  void optimize();
  // Do any possible optimizations on this list.  Currently
  // only applies to subclass CodeList.

  AListIter iter() { return AListIter(List::iter()); }

  template <class T, class... Args>
  T* newNode(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    auto* node_ptr = node.get();
    add(std::move(node));
    return node_ptr;
  }

  template <class T, class... Args>
  T* newNodeBefore(ANode* before, Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    auto node_ptr = node.get();
    addBefore(before, std::move(node));
    return node_ptr;
  }
};

class FixupList : public AList {
  // A FixupList is an AList which has elements in it which need to be relocated
  // by the interpreter at load time.  It builds a table of offsets needing
  // relocation which is appended to the end of the object code being generated.
 public:
  FixupList();
  ~FixupList();

  void clear();
  void list();
  void emit(OutputFile*);
  size_t setOffset(size_t ofs);

  // Increment the number of elements needing fixup.  This is
  // called each time we generate an ANode requiring fixup.

  void initFixups();
  // Called once the module has been compiled to allocate space
  // for the fixup table based on 'numFixups'.

  void listFixups();
  // List the fixup table.

  void emitFixups(OutputFile*);
  // Emit the fixup table to the object file.

  void addFixup(size_t ofs);
  // The word at offset 'ofs' in the object file needs relocation.
  // Add the offset to the fixup table.

 protected:
  std::vector<size_t> fixups;  // storage for fixup values
  size_t fixOfs;               // offset of start of fixups
};

struct CodeList : FixupList {
  // The CodeList class specializes the FixupList class for actual p-code,
  // which is the only sort of AList currently optimized.

  void optimize();
};

extern bool addNodesToList;
extern AList* curList;
extern bool noOptimize;
extern bool shrink;

#endif
