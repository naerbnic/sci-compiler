#ifndef UTIL_TYPES_SEQUENCE_HPP
#define UTIL_TYPES_SEQUENCE_HPP

#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>

#include "absl/base/nullability.h"

namespace util {

namespace internal {

// A pure abstract class that provides the underlying implementation for a
// SeqView. This covers many of the same cases as std::span or absl::Span, but
// does not require the use of templated code.
template <class T>
class SeqViewImpl {
 public:
  virtual ~SeqViewImpl() = default;

  virtual std::size_t size(void* data) const = 0;
  virtual T get_at(void* data, std::size_t index) const = 0;
};

template <class T, class C, class F>
  requires std::is_default_constructible_v<F> &&
           std::ranges::range<std::invoke_result_t<F, C&>>
class RangeSeqViewImpl : public SeqViewImpl<T> {
  using RangeType = std::invoke_result_t<F, C&>;

 public:
  static SeqViewImpl<T> const* Get() {
    static RangeSeqViewImpl const* instance = new RangeSeqViewImpl();
    return instance;
  }

  RangeSeqViewImpl() = default;

  std::size_t size(void* data) const override {
    auto view = ToView(data);
    return std::end(view) - std::begin(view);
  }

  T get_at(void* data, std::size_t index) const override {
    return *(std::begin(ToView(data)) + index);
  }

 private:
  RangeType ToView(void* data) const { return F()(*static_cast<C*>(data)); }
};

}  // namespace internal

// A type-erased sequence view type.
template <class T>
class SeqView {
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

    friend bool operator==(iterator const& lhs, iterator const& rhs) {
      return lhs.parent_ == rhs.parent_ && lhs.index_ == rhs.index_;
    }

   private:
    friend class SeqView;

    iterator(SeqView const* parent, std::size_t index)
        : parent_(parent), index_(index) {}

    SeqView const* parent_;
    std::size_t index_;
  };

  using value_type = T;
  using const_iterator = iterator;

  template <class C, class F>
    requires std::is_default_constructible_v<F>
  static SeqView CreateTransform(C& container, F transform) {
    using ViewTransform = decltype([](C& container) {
      return std::views::transform(container, F());
    });
    return SeqView(&container,
                   internal::RangeSeqViewImpl<T, C, ViewTransform>::Get());
  }

  SeqView() : data_(nullptr), impl_(nullptr){};

  template <class C>
    requires(!std::same_as<C, SeqView>)
  SeqView(C& container)
      : SeqView(
            // We perform a const cast to simplify the common storage of all
            // containers. The method being provided ensures that the container
            // will be converted back to the same constness of container.
            &const_cast<std::remove_const_t<C>&>(container),
            internal::RangeSeqViewImpl<T, C, decltype([](C& container) -> C& {
                                         return container;
                                       })>::Get()) {}

  ~SeqView() = default;
  SeqView(SeqView const&) = default;
  SeqView& operator=(SeqView const&) = default;

  std::size_t size() const {
    if (!impl_) return 0;
    return impl_->size(data_);
  }
  T operator[](std::size_t index) const { return impl_->get_at(data_, index); }

  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, size()); }

 private:
  SeqView(void* data, internal::SeqViewImpl<T> const* impl)
      : data_(data), impl_(impl) {}
  // A pointer to the
  absl::Nullable<void*> data_;
  internal::SeqViewImpl<T> const* impl_;
};

}  // namespace util

#endif