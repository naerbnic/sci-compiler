//	list.cpp		sc

#include "list.hpp"

#include <cassert>

LNode* ListIter::get() { return cur_; }
void ListIter::advance() { cur_ = cur_->next(); }
ListIter::operator bool() { return cur_ != nullptr; }

std::unique_ptr<LNode> ListIter::remove(LNode* ln) {
  bool removing_cur = ln == cur_;
  auto* prev = ln->prev_;
  if (!ln->next_)
    list_->tail = ln->prev_;
  else
    ln->next_->prev_ = ln->prev_;

  if (!ln->prev_)
    list_->head = ln->next_;
  else
    ln->prev_->next_ = ln->next_;

  if (removing_cur) {
    if (prev == nullptr) {
      cur_ = list_->front();
    } else {
      cur_ = prev;
    }
  }
  return std::unique_ptr<LNode>(ln);
}

LNode* ListIter::replaceWith(LNode* ln, std::unique_ptr<LNode> nnp) {
  if (cur_ == ln) {
    cur_ = nnp.get();
  }
  // Take ownership of the node
  auto* nn = nnp.release();
  nn->next_ = ln->next_;
  nn->prev_ = ln->prev_;

  if (!nn->next_)
    list_->tail = nn;
  else
    nn->next_->prev_ = nn;

  if (!nn->prev_)
    list_->head = nn;
  else
    nn->prev_->next_ = nn;

  delete ln;

  return nn;
}

List::List() { head = tail = 0; }

List::~List() { clear(); }

void List::clear() {
  auto* cur = head;
  while (cur) {
    auto* next = cur->next();
    delete cur;
    cur = next;
  };
  head = tail = nullptr;
}

void List::add(std::unique_ptr<LNode> lnp) {
  auto* ln = lnp.release();
  ln->next_ = 0;
  ln->prev_ = tail;
  if (tail) tail->next_ = ln;
  tail = ln;
  if (!head) head = ln;
}

void List::addFront(std::unique_ptr<LNode> lnp) {
  auto* ln = lnp.release();
  if (head) head->prev_ = ln;
  ln->next_ = head;

  head = ln;
  ln->prev_ = 0;

  if (!tail) tail = ln;
}

void List::addAfter(LNode* ln, std::unique_ptr<LNode> nnp) {
  if (!ln) {
    addFront(std::move(nnp));
    return;
  }

  auto* nn = nnp.release();

  nn->next_ = ln->next_;
  if (nn->next_) nn->next_->prev_ = nn;

  nn->prev_ = ln;
  ln->next_ = nn;

  if (ln == tail) tail = nn;
}

void List::addBefore(LNode* ln, std::unique_ptr<LNode> nnp) {
  if (!ln) {
    add(std::move(nnp));
    return;
  }

  auto* nn = nnp.release();

  nn->next_ = ln;
  nn->prev_ = ln->prev_;

  if (ln->prev_) ln->prev_->next_ = nn;
  ln->prev_ = nn;

  if (ln == head) head = nn;
}

bool List::contains(LNode* ln) {
  if (!ln) return false;

  LNode* node;
  for (node = head; node && node != ln; node = node->next_);

  return node == ln;
}

ListIter List::iter() { return ListIter(this, head); }

// ---------------

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

bool TListBase::contains(TNode* ln) { return ln->list_ == this; }