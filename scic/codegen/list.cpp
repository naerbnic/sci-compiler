//	list.cpp		sc

#include "scic/codegen/list.hpp"

#include <cassert>

// ---------------

namespace codegen {

TNode::TNode() : list_(nullptr), next_(nullptr), prev_(nullptr) {}
TNode::~TNode() { RemoveFromList(); }

TNode** TNode::next_to_this() { return prev_ ? &prev_->next_ : &list_->head_; }
TNode** TNode::prev_to_this() { return next_ ? &next_->prev_ : &list_->tail_; }

bool TNode::IsInList() { return list_ != nullptr; }
void TNode::RemoveFromList() {
  if (!list_) return;
  *next_to_this() = next_;
  *prev_to_this() = prev_;

  list_ = nullptr;
  prev_ = next_ = nullptr;
}

void TNode::InsertAfter(TNode* ln) {
  assert(!ln->IsInList());
  // Set fields in inserted node
  ln->list_ = list_;

  ln->next_ = next_;
  ln->prev_ = this;

  // If it exists, set fields in next node
  *prev_to_this() = ln;

  // Set our field last, to ensure other methods have expected values.
  next_ = ln;
}

void TNode::InsertBefore(TNode* ln) {
  assert(!ln->IsInList());
  // Set fields in inserted node
  ln->list_ = list_;
  ln->prev_ = prev_;
  ln->next_ = this;

  // If it exists, set fields in next node
  *next_to_this() = ln;

  // Set our field last, to ensure other methods have expected values.
  prev_ = ln;
}

void TNode::ReplaceWith(TNode* ln) {
  assert(!ln->IsInList());
  ln->list_ = list_;
  ln->next_ = next_;
  ln->prev_ = prev_;

  *next_to_this() = ln;
  *prev_to_this() = ln;

  list_ = nullptr;
  prev_ = next_ = nullptr;
}

// ---------------

void TListBase::addBack(TNode* ln) {
  assert(!ln->IsInList());
  ln->list_ = this;
  ln->next_ = nullptr;
  ln->prev_ = tail_;
  if (tail_)
    tail_->next_ = ln;
  else
    head_ = ln;
  tail_ = ln;
}

// Add ln to the head of the list.
void TListBase::addFront(TNode* ln) {
  assert(!ln->IsInList());
  ln->list_ = this;
  ln->next_ = head_;
  ln->prev_ = nullptr;
  if (head_)
    head_->prev_ = ln;
  else
    tail_ = ln;
  head_ = ln;
}

TNode* TListBase::removeFront() {
  if (!head_) return nullptr;
  auto* node = head_;
  node->RemoveFromList();
  return node;
}

TNode* TListBase::removeBack() {
  if (!tail_) return nullptr;
  auto* node = tail_;
  node->RemoveFromList();
  return node;
}

bool TListBase::contains(TNode const* ln) {
  if (!ln) return false;
  return ln->list_ == this;
}

}  // namespace codegen