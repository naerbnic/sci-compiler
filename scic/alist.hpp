//	alist.hpp
// 	definitions for assembly

#ifndef ALIST_HPP
#define ALIST_HPP

#include <cstddef>
#include <memory>
#include <utility>

#include "scic/anode.hpp"
#include "scic/list.hpp"
#include "scic/listing.hpp"

template <std::derived_from<ANode> T>
class AList {
 public:
  // AList (assembly list) is a list of ANodes (assembly nodes).

  size_t length() {
    size_t s = 0;
    for (auto it = iter(); it; ++it) s++;
    return s;
  }

  // Set the offsets of each element of the list based on the
  // start of the list being at offset 'ofs'.

  TList<T>::iterator iter() { return list_.begin(); }

  TList<T>::iterator begin() { return list_.begin(); }
  TList<T>::iterator end() { return list_.end(); }

  TList<T>::iterator find(T* node) { return list_.findIter(node); }

  T* addFront(std::unique_ptr<T> node) {
    return list_.addFront(std::move(node));
  }

  template <class U, class... Args>
    requires std::convertible_to<U*, T*>
  U* newNode(Args&&... args) {
    auto node = std::make_unique<U>(std::forward<Args>(args)...);
    auto* node_ptr = node.get();
    list_.addBack(std::move(node));
    return node_ptr;
  }

 private:
  TList<T> list_;
};

using ANodeList = AList<ANode>;
using AOpList = AList<ANOpCode>;

template <class T>
struct ANComposite : ANode {
 public:
  size_t size() override {
    size_t s = 0;
    for (auto& node : list_) s += node.size();
    return s;
  }

  size_t setOffset(size_t ofs) override {
    offset = ofs;
    for (auto& node : list_) {
      ofs = node.setOffset(ofs);
    }

    return ofs;
  }

  bool tryShrink() override {
    bool changed = false;
    for (auto& node : list_) {
      changed |= node.tryShrink();
    }
    return changed;
  }

  void list(ListingFile* listFile) override {
    for (auto& node : list_) {
      node.list(listFile);
    }
  }

  void collectFixups(FixupContext* fixup_ctxt) override {
    for (auto& node : list_) {
      node.collectFixups(fixup_ctxt);
    }
  }

  void emit(OutputFile* out) override {
    for (auto& node : list_) {
      node.emit(out);
    }
  }

  bool contains(ANode* node) override {
    if (ANode::contains(node)) return true;
    for (auto& entry : list_) {
      if (entry.contains(node)) return true;
    }
    return false;
  }

  bool optimize() override {
    bool changed = false;
    for (auto& node : list_) {
      changed |= node.optimize();
    }
    return changed;
  }

  AList<T>* getList() { return &list_; }

 private:
  AList<T> list_;
};

#endif
