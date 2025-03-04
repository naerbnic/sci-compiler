//	alist.hpp
// 	definitions for assembly

#ifndef ALIST_HPP
#define ALIST_HPP

#include <cstddef>
#include <memory>
#include <utility>

#include "scic/codegen/anode.hpp"
#include "scic/codegen/list.hpp"
#include "scic/codegen/output.hpp"
#include "scic/listing.hpp"

namespace codegen {

template <std::derived_from<ANode> T>
class AList {
 public:
  // AList (assembly list) is a list of ANodes (assembly nodes).

  size_t length() const {
    size_t s = 0;
    for (auto it = iter(); it; ++it) s++;
    return s;
  }

  TList<T>::iterator iter() { return list_.begin(); }
  TList<T>::const_iterator iter() const { return list_.begin(); }

  TList<T>::iterator begin() { return list_.begin(); }
  TList<T>::iterator end() { return list_.end(); }

  TList<T>::const_iterator begin() const { return list_.begin(); }
  TList<T>::const_iterator end() const { return list_.end(); }

  TList<T>::iterator find(T* node) { return list_.findIter(node); }
  TList<T>::const_iterator find(T const* node) const {
    return list_.findIter(node);
  }

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
  size_t size() const override {
    size_t s = 0;
    for (auto const& node : list_) s += node.size();
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

  void list(ListingFile* listFile) const override {
    for (auto const& node : list_) {
      node.list(listFile);
    }
  }

  void collectFixups(FixupContext* fixup_ctxt) const override {
    for (auto const& node : list_) {
      node.collectFixups(fixup_ctxt);
    }
  }

  void emit(OutputWriter* out) const override {
    for (auto const& node : list_) {
      node.emit(out);
    }
  }

  bool contains(ANode const* node) const override {
    if (ANode::contains(node)) return true;
    for (auto const& entry : list_) {
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

}  // namespace codegen

#endif
