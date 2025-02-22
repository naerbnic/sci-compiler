#ifndef UTIL_STRINGS_REF_STR_HPP
#define UTIL_STRINGS_REF_STR_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace util {
class RefStr;
namespace ref_str_literals {
RefStr operator""_rs(char const* str, std::size_t len);
}  // namespace ref_str_literals

// A reference to a constant shared string.
//
// The string value itself, once created, cannot be changed.
class RefStr {
 public:
  // Default. Constructs an empty string.
  RefStr();

  // Construct from any type that can be converted to a string_view.
  //
  // This will copy the string into a shared buffer.
  explicit RefStr(std::string_view str);

  std::string_view view() const;

  // Converts the reference to a string_view.
  operator std::string_view() const;

  // Basic comparisons
  bool operator==(RefStr const& other) const {
    return std::string_view(*this) == std::string_view(other);
  }

  auto operator<=>(RefStr const& other) const {
    return std::string_view(*this) <=> std::string_view(other);
  }

  // These are necessary to disambiguate the comparison, since we have an
  // implicit conversion from a string literal.
  template <std::size_t N>
  bool operator==(const char (&other)[N]) const {
    return std::string_view(*this) == std::string_view(other);
  }

  template <std::size_t N>
  auto operator<=>(const char (&other)[N]) const {
    return std::string_view(*this) <=> other;
  }

  // Directo comparisons to string_view.
  bool operator==(std::string_view const& other) const {
    return std::string_view(*this) == other;
  }

  auto operator<=>(std::string_view const& other) const {
    return std::string_view(*this) <=> other;
  }

 private:
  friend RefStr util::ref_str_literals::operator""_rs(char const* str,
                                                      std::size_t len);
  class Impl;

  // An internal constructor for a string literal.
  explicit constexpr RefStr(std::in_place_type_t<std::string_view>,
                            std::string_view str)
      : value_(str) {}

  static std::shared_ptr<Impl> MakeImpl(std::string_view str);

  void SetString(std::string_view str);

  // The string_view is only used if this value was initialized with a string
  // literal.
  std::variant<std::string_view, std::shared_ptr<Impl>> value_;

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