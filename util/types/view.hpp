#ifndef UTIL_TYPES_VIEW_HPP
#define UTIL_TYPES_VIEW_HPP

#include <cstddef>
#include <functional>
#include <ostream>
#include <type_traits>

#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/has_ostream_operator.h"
#include "absl/strings/str_format.h"

namespace util {

// A type that borrows a reference to a value. This is similar to a non-null
// pointer, but provides some useful properties:
//
// - Provides comparison operators if the underlying type does. (as opposed to
//   pointer comparison)
// - Provides support for ostream printing and AbslStringify support
// - Has simple copy semantics (unlike references)
//
template <class T>
class View {
  static_assert(!std::is_reference_v<T>,
                "View<T> cannot be used with a reference type");

 public:
  using value_type = T;

  View(T& value) : value_ptr_(&value) {}
  ~View() = default;

  View(View const&) = default;
  View& operator=(View const&) = default;
  View(View&&) = default;
  View& operator=(View&&) = default;

  T& value() { return *value_ptr_; }
  T const& value() const { return *value_ptr_; }

  T* operator->() { return value_ptr_; }
  T const* operator->() const { return value_ptr_; }
  T& operator*() { return *value_ptr_; }
  T const& operator*() const { return *value_ptr_; }

  bool operator==(View const& other) const
    requires std::equality_comparable<T>
  {
    return value() == other.value();
  }

  auto operator<=>(View const& other) const
    requires std::three_way_comparable<T>
  {
    return value() <=> other.value();
  }

  friend std::ostream& operator<<(std::ostream& os, View const& view)
    requires(absl::HasOstreamOperator<T>::value)
  {
    return os << view.value();
  }

  template <class Sink>
  friend void AbslStringify(Sink& sink, View const& view)
    requires(absl::HasAbslStringify<T>::value)
  {
    absl::Format(&sink, "%v", view.value());
  }

  template <class H>
  friend H AbslHashValue(H h, View const& view) {
    return H::combine(std::move(h), view.value());
  }

 private:
  T* value_ptr_;
};

}  // namespace util

template <class T>
struct std::hash<util::View<T>> {
  std::size_t operator()(util::View<T> const& view) const {
    return std::hash<T>()(view.value());
  }
};

#endif