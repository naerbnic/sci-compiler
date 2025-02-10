#ifndef SCIC_PARSERS_SCI_AST_HPP
#define SCIC_PARSERS_SCI_AST_HPP

#include <memory>
#include <type_traits>
#include <utility>

#include "scic/tokenizer/text_contents.hpp"

namespace parsers::sci {

namespace internal {

template <class T>
using PtrElementType = std::remove_reference_t<decltype(*std::declval<T&>())>;

template <class T>
concept IsSmartPtr = requires(T& t) {
  typename PtrElementType<T>;
  { std::to_address(t) } -> std::convertible_to<PtrElementType<T>*>;
  { t.operator->() } -> std::convertible_to<PtrElementType<T>*>;
  { *t } -> std::convertible_to<PtrElementType<T>&>;
  { t.get() } -> std::convertible_to<PtrElementType<T>*>;
} && requires(T const& t) {
  typename PtrElementType<T const>;
  { std::to_address(t) } -> std::convertible_to<PtrElementType<T const>*>;
  { t.operator->() } -> std::convertible_to<PtrElementType<T const>*>;
  { *t } -> std::convertible_to<PtrElementType<T const>&>;
  { t.get() } -> std::convertible_to<PtrElementType<T const>*>;
};

template <class T>
concept IsPtr = IsSmartPtr<T> || std::is_pointer_v<T>;

template <class T>
struct NodeElementType {
  using type = T;
};

template <class T>
  requires IsPtr<T>
struct NodeElementType<T> {
  using type = typename std::pointer_traits<T>::element_type;
};

}  // namespace internal

// A value that carries provenance information about which file it comes
// from. This can be used to provide better error messages.
template <class T>
class TokenNode {
 public:
  using element_type = internal::NodeElementType<T>::type;
  TokenNode(T value, tokenizer::TextRange text_range)
      : value_(std::move(value)), text_range_(std::move(text_range)) {}

  T const& value() const { return value_; }
  tokenizer::TextRange const& text_range() const { return text_range_; }

  // Act as a smart pointer to the value. If the internal type is a pointer,
  // act as if it were transparent.
  explicit operator bool() const { return bool(get_ptr()); }
  element_type* operator->() { return get_ptr(); }
  element_type const* operator->() const { return get_ptr(); }
  element_type& operator*() { return *get_ptr(); }
  element_type const& operator*() const { return *get_ptr(); }
  element_type* get() { return get_ptr(); }
  element_type const* get() const { return get_ptr(); }

  template <class U = T>
    requires internal::IsPtr<U>
  explicit operator bool() const {
    return bool(value_);
  }

  template <class U = T>
    requires(!internal::IsPtr<U>)
  explicit operator bool() const {
    return true;
  }

 private:
  template <class U = T>
    requires internal::IsPtr<U>
  element_type* get_ptr() {
    return std::to_address(value_);
  }

  template <class U = T>
    requires internal::IsPtr<U>
  element_type const* get_ptr() const {
    return std::to_address(value_);
  }

  template <class U = T>
    requires(!internal::IsPtr<U>)
  element_type* get_ptr() {
    return &value_;
  }

  template <class U = T>
    requires(!internal::IsPtr<U>)
  element_type const* get_ptr() const {
    return &value_;
  }

  T value_;
  tokenizer::TextRange text_range_;
};

}  // namespace parsers::sci

#endif