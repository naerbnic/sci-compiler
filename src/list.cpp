//	list.cpp		sc

#include "list.hpp"

#include "sc.hpp"
#include "sol.hpp"

List::List() { head = tail = cur = 0; }

List::~List() { clear(); }

void List::clear() {
  while (head) del(head);
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

std::unique_ptr<LNode> List::remove(LNode* ln) {
  if (!ln->next_)
    tail = ln->prev_;
  else
    ln->next_->prev_ = ln->prev_;

  if (!ln->prev_)
    head = ln->next_;
  else
    ln->prev_->next_ = ln->next_;

  if (cur == ln) cur = ln->prev_;
  if (!cur) cur = head;

  return std::unique_ptr<LNode>(ln);
}

void List::del(LNode* ln) { remove(ln); }

LNode* List::replaceWith(LNode* ln, std::unique_ptr<LNode> nnp) {
  // Take ownership of the node
  auto* nn = nnp.release();
  nn->next_ = ln->next_;
  nn->prev_ = ln->prev_;

  if (!nn->next_)
    tail = nn;
  else
    nn->next_->prev_ = nn;

  if (!nn->prev_)
    head = nn;
  else
    nn->prev_->next_ = nn;

  if (cur == ln) cur = nn;

  delete ln;

  return nn;
}

bool List::contains(LNode* ln) {
  if (!ln) return False;

  LNode* node;
  for (node = head; node && node != ln; node = node->next_);

  return node == ln;
}

LNode* List::first() {
  cur = head;
  return cur;
}

LNode* List::next() {
  if (cur) cur = cur->next_;
  return cur;
}
