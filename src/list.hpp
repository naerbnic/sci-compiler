//	list.hpp		sc
// 	definition of a list class

#ifndef LIST_HPP
#define LIST_HPP

#include <list>
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

template <class T>
class TList {
  using BaseList = std::list<std::unique_ptr<T>>;

 public:
  TList() : list_(std::make_unique<BaseList>()) {}

  // Smart pointer to a list node.
  class Ref {
   public:
    explicit operator bool() const { return iterator_ != parent_->end(); }
    T* operator->() const { return iterator_->get(); }
    T* get() const { return iterator_->get(); }
    T& operator*() const { return *get(); }

    bool operator==(const Ref& other) const {
      if (!parent_ == other.parent_) return false;
      return iterator_ == other.iterator_;
    }

    std::optional<Ref> next() const {
      auto next_iter = iterator_;
      ++next_iter;
      if (next_iter == parent_->end()) {
        return std::nullopt;
      }
      return Ref(parent_, next_iter);
    }

    std::optional<Ref> prev() const {
      if (iterator_ == parent_->begin()) {
        return std::nullopt;
      }
      auto prev_iter = iterator_;
      --prev_iter;
      return Ref(parent_, prev_iter);
    }

    // Add node nn before node ln in the list.
    Ref addBefore(LNode* ln, std::unique_ptr<T> nn) {
      auto it = parent_->emplace(iterator_, std::move(nn));
      return Ref(parent_, it);
    }

    Ref addAfter(std::unique_ptr<T> nn) {
      auto it = parent_->emplace(iterator_, std::move(nn));
      ++it;
      return Ref(parent_, it);
    }

   private:
    Ref(BaseList* parent, BaseList::iterator iterator)
        : parent_(parent), iterator_(iterator) {}
    friend class TList;
    BaseList* parent_;
    // A null element is the end of the list.
    BaseList::iterator iterator_;
  };

  // Delete all elements in the list.
  void clear() { list_->clear(); }

  // Add ln to the tail of the list.
  Ref add(std::unique_ptr<T> ln) {
    auto it = list_->emplace(list_.end(), std::move(ln));
    return Ref(list_.get(), it);
  }

  // Add ln to the head of the list.
  Ref addFront(std::unique_ptr<T> ln) {
    auto it = list_.emplace(list_.begin(), std::move(ln));
    return Ref(list_.get(), it);
  }

  Ref contains(T* ln) {
    auto it = std::ranges::find_if(*list_, [ln](const std::unique_ptr<T>& ptr) {
      return ptr.get() == ln;
    });
    return Ref(list_.get(), it);
  }

 private:
  std::unique_ptr<BaseList> list_;
};

#endif
