//	list.hpp		sc
// 	definition of a list class

#ifndef LIST_HPP
#define LIST_HPP

#include <cassert>
#include <memory>

#include "scic/casts.hpp"

class TListBase;

class TNode {
 public:
  TNode();
  virtual ~TNode();

 private:
  friend class TListBase;

  template <class T>
  friend class TList;

  // A pointer to the next field pointing to this object;
  //
  // If this is the head node, this is the next field of the head node.
  TNode** next_to_this();

  // A pointer to the prev field pointing to this object.j
  //
  // If this is the tail node, this is the prev field of the tail node.
  TNode** prev_to_this();

  bool IsInList();
  void RemoveFromList();
  void InsertAfter(TNode* ln);
  void InsertBefore(TNode* ln);
  void ReplaceWith(TNode* ln);

  TListBase* list_;
  TNode* next_;
  TNode* prev_;
};

class TListBase {
 public:
  TListBase() : head_(nullptr), tail_(nullptr) {}

  // Add ln to the tail of the list.
  void addBack(TNode* ln);

  // Add ln to the head of the list.
  void addFront(TNode* ln);

  TNode* front() { return head_; }
  TNode* back() { return tail_; }

  TNode* removeFront();
  TNode* removeBack();

  // Returns true if this node is in this list.
  bool contains(TNode* ln);

 private:
  friend class TNode;
  template <class T>
  friend class TList;

  TNode* head_;
  TNode* tail_;
};

template <class T>
class TList {
 public:
  TList() : list_(std::make_unique<TListBase>()) {}

  // Smart pointer to a list node.
  class iterator {
   public:
    using value_type = T;

    // This should ultimately be unused, as the difference between two
    // iterators is not O(1), and won't be implemented unless necessary.
    using difference_type = std::ptrdiff_t;
    iterator() : parent_(nullptr), curr_item_(nullptr) {}

    T* operator->() const { return curr_item_; }
    T* get() const { return curr_item_; }
    T& operator*() const { return *curr_item_; }

    explicit operator bool() const { return curr_item_ != nullptr; }

    bool operator==(const iterator& other) const {
      if (parent_ != other.parent_) return false;
      return curr_item_ == other.curr_item_;
    }

    bool operator!=(const iterator& other) const { return !(*this == other); }

    iterator& operator++() {
      advance();
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      advance();
      return tmp;
    }

    iterator& operator--() {
      retreat();
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      retreat();
      return tmp;
    }

    iterator next() const {
      iterator tmp = *this;
      tmp.advance();
      return tmp;
    }

    iterator prev() const {
      iterator tmp = *this;
      tmp.retreat();
      return tmp;
    }

    // Add node nn before node ln in the list.
    void addBefore(std::unique_ptr<T> nn) {
      if (curr_item_ == nullptr) {
        parent_->addBack(nn.release());
      } else {
        curr_item_->InsertBefore(nn.release());
      }
    }

    void addAfter(std::unique_ptr<T> nn) {
      assert(curr_item_);
      curr_item_->InsertAfter(nn.release());
    }

    std::unique_ptr<T> remove() {
      assert(curr_item_);
      auto* next = curr_item_->next_;
      curr_item_->RemoveFromList();
      return std::unique_ptr<T>(curr_item_);
      curr_item_ = down_cast<T>(next);
    }

    std::unique_ptr<T> replaceWith(std::unique_ptr<T> nn) {
      assert(curr_item_);
      auto* removed_node = curr_item_;
      auto* new_node = nn.release();
      removed_node->ReplaceWith(new_node);
      curr_item_ = new_node;
      return std::unique_ptr<T>(removed_node);
    }

   private:
    friend class TList;

    iterator(TListBase* parent, T* curr_item)
        : parent_(parent), curr_item_(curr_item) {}

    void advance() {
      assert(curr_item_);
      curr_item_ = down_cast<T>(curr_item_->next_);
    }

    void retreat() {
      assert(curr_item_ != parent_->front());
      if (curr_item_ == nullptr) {
        curr_item_ = down_cast<T>(parent_->back());
      } else {
        curr_item_ = down_cast<T>(curr_item_->prev_);
      }
    }

    TListBase* parent_;
    // A null element is the end of the list.
    T* curr_item_;
  };

  static_assert(std::bidirectional_iterator<iterator>);

  iterator begin() {
    return iterator(list_.get(), down_cast<T>(list_->front()));
  }
  iterator end() { return iterator(list_.get(), nullptr); }

  iterator findIter(T* ln) {
    if (!list_->contains(ln)) return end();
    return iterator(list_.get(), ln);
  }

  T* frontPtr() { return down_cast<T>(list_->front()); }

  // Delete all elements in the list.
  void clear() {
    while (true) {
      TNode* ln = list_->removeFront();
      if (!ln) break;
      delete ln;
    }
  }

  // Add ln to the tail of the list.
  T* addBack(std::unique_ptr<T> ln) {
    auto* node = ln.release();
    list_->addBack(node);
    return node;
  }

  // Add ln to the head of the list.
  T* addFront(std::unique_ptr<T> ln) {
    auto* node = ln.release();
    list_->addFront(node);
    return node;
  }

  bool contains(T* ln) { return list_->contains(ln); }

 private:
  std::unique_ptr<TListBase> list_;
};

#endif
