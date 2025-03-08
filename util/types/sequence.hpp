#ifndef UTIL_TYPES_SEQUENCE_HPP
#define UTIL_TYPES_SEQUENCE_HPP

#include <compare>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>

#include "absl/base/nullability.h"

namespace util {

namespace internal {

template <class C>
concept IsIndexable = requires(C& c, std::size_t i) {
  { c.size() } -> std::convertible_to<std::size_t>;
  { c[i] };
};

template <class T, class C, class F>
concept IsIndexSequence = IsIndexable<C> && requires(C& c, std::size_t i) {
  { F()(c[i]) } -> std::convertible_to<T>;
};

// A pure abstract class that provides the underlying implementation for a
// SeqView. This covers many of the same cases as std::span or absl::Span, but
// does not require the use of templated code.
template <class T>
class SeqImpl {
 public:
  virtual ~SeqImpl() = default;

  virtual std::size_t size(void* data) const = 0;
  virtual T get_at(void* data, std::size_t index) const = 0;
};

// An implementation of SeqViewImpl that is based on a base object and a
// functor that transforms it into a range.
//
// The functor must be trivially default constructable, and must return a range
// object. The assumption is that the functor's operation is trivial, and can
// be optimized into direct line code.
template <class T, class C, class F>
  requires std::is_trivially_default_constructible_v<F> &&
           std::invocable<F, C&> &&
           std::ranges::random_access_range<std::invoke_result_t<F, C&>>
class RangeSeqImpl : public SeqImpl<T> {
  using RangeType = std::invoke_result_t<F, C&>;

 public:
  static SeqImpl<T> const* Get() {
    static RangeSeqImpl const* instance = new RangeSeqImpl();
    return instance;
  }

  std::size_t size(void* data) const override {
    auto&& view = ToView(data);
    return std::end(view) - std::begin(view);
  }

  T get_at(void* data, std::size_t index) const override {
    return *(std::begin(ToView(data)) + index);
  }

 private:
  RangeSeqImpl() = default;
  RangeType ToView(void* data) const { return F()(*static_cast<C*>(data)); }
};

template <class T, class C, class F>
  requires std::is_trivially_default_constructible_v<F> &&
           IsIndexSequence<T, C, F>
class IndexableSetViewImpl : public SeqImpl<T> {
 public:
  static SeqImpl<T> const* Get() {
    static IndexableSetViewImpl const* instance = new IndexableSetViewImpl();
    return instance;
  }

  IndexableSetViewImpl() = default;

  std::size_t size(void* data) const override {
    return static_cast<C*>(data)->size();
  }

  T get_at(void* data, std::size_t index) const override {
    return F()((*static_cast<C*>(data))[index]);
  }

 private:
};

template <class T>
class SingletonSeqViewImpl : public SeqImpl<T> {
 public:
  static SeqImpl<T> const* Get() {
    static SingletonSeqViewImpl const* instance = new SingletonSeqViewImpl();
    return instance;
  }

  std::size_t size(void* data) const override { return 1; }

  T get_at(void* data, std::size_t index) const override {
    return *static_cast<std::remove_reference_t<T>*>(data);
  }
};

template <class T>
struct identity_func_t {
  auto operator()(T c) -> decltype(auto) { return c; }
};

}  // namespace internal

// A type-erased sequence view type.
template <class T>
class Seq {
 public:
  class iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    iterator() : parent_(nullptr), index_(0) {}
    iterator(iterator const&) = default;
    iterator& operator=(iterator const&) = default;

    T operator*() const { return (*parent_)[index_]; }

    iterator& operator++() {
      ++index_;
      return *this;
    }

    iterator operator++(int) {
      auto copy = *this;
      ++index_;
      return copy;
    }

    iterator& operator--() {
      --index_;
      return *this;
    }

    iterator operator--(int) {
      auto copy = *this;
      --index_;
      return copy;
    }

    iterator& operator+=(std::ptrdiff_t n) {
      index_ += n;
      return *this;
    }

    iterator& operator-=(std::ptrdiff_t n) {
      index_ -= n;
      return *this;
    }

    iterator operator+(std::ptrdiff_t n) const {
      return iterator(parent_, index_ + n);
    }

    friend iterator operator+(std::ptrdiff_t n, iterator const& it) {
      return it + n;
    }

    iterator operator-(std::ptrdiff_t n) const {
      return iterator(parent_, index_ - n);
    }

    T operator[](std::ptrdiff_t n) const { return (*parent_)[index_ + n]; }

    std::ptrdiff_t operator-(iterator const& rhs) const {
      if (parent_ != rhs.parent_) {
        throw std::logic_error("Comparing iterators from different SeqViews");
      }
      return index_ - rhs.index_;
    }

    bool operator==(iterator const& rhs) const {
      if (parent_ != rhs.parent_) {
        throw std::logic_error("Comparing iterators from different SeqViews");
      }
      return index_ == rhs.index_;
    }

    std::strong_ordering operator<=>(iterator const& rhs) const {
      if (parent_ != rhs.parent_) {
        throw std::logic_error("Comparing iterators from different SeqViews");
      }
      return index_ <=> rhs.index_;
    }

   private:
    friend class Seq;

    iterator(Seq const* parent, std::size_t index)
        : parent_(parent), index_(index) {}

    Seq const* parent_;
    std::size_t index_;
  };

  using value_type = T;
  using const_iterator = iterator;

  template <class C, class F>
    requires std::is_default_constructible_v<F>
  static Seq CreateTransform(C& container, F transform) {
    using ViewTransform = decltype([](C& container) {
      return std::views::transform(container, F());
    });
    return Seq(&const_cast<std::remove_const_t<C>&>(container),
               internal::RangeSeqImpl<T, C, ViewTransform>::Get());
  }

  template <class C>
  static Seq Deref(C& container) {
    return Seq::CreateTransform(
        container, [](auto& value) -> decltype(auto) { return *value; });
  }

  static Seq Singleton(T& value) {
    return Seq(&const_cast<std::remove_cvref_t<T>&>(value),
               internal::SingletonSeqViewImpl<T>::Get());
  }

  Seq() : data_(nullptr), impl_(nullptr){};

  template <class C>
    requires(!std::same_as<C, Seq> && std::ranges::random_access_range<C&>)
  Seq(C& container)
      : Seq(
            // We perform a const cast to simplify the common storage of all
            // containers. The method being provided ensures that the container
            // will be converted back to the same constness of container.
            &const_cast<std::remove_const_t<C>&>(container),
            internal::RangeSeqImpl<T, C,
                                   internal::identity_func_t<C&>>::Get()) {}

  template <class C>
    requires(!std::same_as<C, Seq> && !std::ranges::random_access_range<C&> &&
             internal::IsIndexSequence<T, C, internal::identity_func_t<T>>)
  Seq(C& container)
      : Seq(&container, internal::IndexableSetViewImpl<
                            T, C, internal::identity_func_t<T>>::Get()) {}

  ~Seq() = default;
  Seq(Seq const&) = default;
  Seq& operator=(Seq const&) = default;

  std::size_t size() const {
    if (!impl_) return 0;
    return impl_->size(data_);
  }
  T operator[](std::size_t index) const { return impl_->get_at(data_, index); }

  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, size()); }

 private:
  Seq(void* data, internal::SeqImpl<T> const* impl)
      : data_(data), impl_(impl) {}
  // A pointer to the
  absl::Nullable<void*> data_;
  internal::SeqImpl<T> const* impl_;
};

// A sequence that is backed by references to the original data.
template <class T>
using SeqView = Seq<T&>;

}  // namespace util

#endif