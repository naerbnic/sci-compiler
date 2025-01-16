//	list.cpp		sc

#include "list.hpp"

#include "sc.hpp"
#include "sol.hpp"

List::List() { head = tail = cur = 0; }

List::~List() { clear(); }

void List::clear() {
  while (head) del(head);
}

void List::add(LNode* ln) {
  ln->next_ = 0;
  ln->prev_ = tail;
  if (tail) tail->next_ = ln;
  tail = ln;
  if (!head) head = ln;
}

void List::addFront(LNode* ln) {
  if (head) head->prev_ = ln;
  ln->next_ = head;

  head = ln;
  ln->prev_ = 0;

  if (!tail) tail = ln;
}

void List::addAfter(LNode* ln, LNode* nn) {
  if (!ln) {
    addFront(nn);
    return;
  }

  nn->next_ = ln->next_;
  if (nn->next_) nn->next_->prev_ = nn;

  nn->prev_ = ln;
  ln->next_ = nn;

  if (ln == tail) tail = nn;
}

void List::addBefore(LNode* ln, LNode* nn) {
  if (!ln) {
    add(nn);
    return;
  }

  nn->next_ = ln;
  nn->prev_ = ln->prev_;

  if (ln->prev_) ln->prev_->next_ = nn;
  ln->prev_ = nn;

  if (ln == head) head = nn;
}

void List::remove(LNode* ln) {
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
}

void List::del(LNode* ln) {
  remove(ln);
  delete ln;
}

LNode* List::replaceWith(LNode* ln, LNode* nn) {
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
