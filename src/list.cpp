//	list.cpp		sc

#include "list.hpp"

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
