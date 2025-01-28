//	list.hpp		sc
// 	definition of a list class

#ifndef LIST_HPP
#define LIST_HPP

#include <memory>

class List;
class ListIter;

struct LNode {
  // Class LNode is the base class for any class to be linked in a doubly
  // linked list descended from List.

  LNode() : next_(nullptr), prev_(nullptr) {}
  virtual ~LNode() {};

  LNode* next() { return next_; }

 private:
  friend class List;
  friend class ListIter;
  LNode* next_;
  LNode* prev_;
};

class ListIter {
 public:
  LNode* get();
  void advance();
  explicit operator bool();

  // Remove node ln from the list. Returns a unique_ptr to the returned node.
  std::unique_ptr<LNode> remove(LNode* ln);

  // Replace node ln with node nn, return a pointer to node nn.
  LNode* replaceWith(LNode* ln, std::unique_ptr<LNode> nn);

 private:
  friend class List;

  explicit ListIter(List* list, LNode* cur) : list_(list), cur_(cur) {}
  List* list_;
  LNode* cur_;
};

class List {
  // Class List implements a doubly linked list.
 public:
  List();
  ~List();

  void clear();
  // Uses del() to remove each element from the list and delete it.

  void add(std::unique_ptr<LNode> ln);
  // Add ln to the tail of the list.

  void addFront(std::unique_ptr<LNode> ln);
  // Add ln to the head of the list.

  void addAfter(LNode* ln, std::unique_ptr<LNode> nn);
  // Add node nn after node ln in the list.

  void addBefore(LNode* ln, std::unique_ptr<LNode> nn);
  // Add node nn before node ln in the list.

  bool contains(LNode* ln);
  // Return true if the list contains node ln, false otherwise.

  ListIter iter();

  LNode* front() { return head; }

 protected:
  friend ListIter;
  LNode* head;  // head of the list
  LNode* tail;  // tail of the list
};

#endif
