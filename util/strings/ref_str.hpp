#ifndef UTIL_STRINGS_REF_STR_HPP
#define UTIL_STRINGS_REF_STR_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <ostream>
#include <string_view>
#include <type_traits>

namespace util {

namespace internal {

template <std::size_t N>
using ConstCharArray = const char[N];
template <std::size_t N>
struct LitStr {
  constexpr LitStr(ConstCharArray<N> const& str) : str_(str) {}
  const char* str_;
};

template <std::size_t N>
LitStr(ConstCharArray<N> const&) -> LitStr<N>;
}  // namespace internal

// A reference to a constant shared string.
//
// The string value itself, once created, cannot be changed.
class RefStr {
 public:
  // Default. Constructs an empty string.
  RefStr();

  // Implicitly construct from a string literal.
  template <std::size_t N>
  RefStr(const char (&str)[N]) : RefStr(std::string_view(&str[0], N - 1)) {}

  // Construct from any type that can be converted to a string_view.
  //
  // This will copy the string into a shared buffer.
  explicit RefStr(std::string_view str);

  std::string_view view() const;

  // Converts the reference to a string_view.
  operator std::string_view() const;

  bool operator==(std::string_view const& other) const {
    return std::string_view(*this) == other;
  }

  auto operator<=>(std::string_view const& other) const {
    return std::string_view(*this) <=> other;
  }

 private:
  class Impl;

  static std::shared_ptr<Impl> MakeImpl(std::string_view str);

  void SetString(std::string_view str);

  // The string_view is only used if this value was initialized with a string
  // literal.
  std::shared_ptr<Impl> value_;

  // For Absl hash collection types.
  template <typename H>
  friend H AbslHashValue(H h, RefStr const& c) {
    return H::combine(c.view());
  }

  friend std::ostream& operator<<(std::ostream& os, RefStr const& str) {
    return os << str.view();
  }
};

}  // namespace util

// Allow for strings as hash keys.
template <>
struct ::std::hash<::util::RefStr> {
  std::size_t operator()(::util::RefStr const& str) const {
    return std::hash<::std::string_view>{}(str);
  }
};

#endif