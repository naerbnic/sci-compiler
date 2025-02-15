// Gets the user-readable name of a type, for messages in template code.
//
// This requires some multi-platform code, as different compilers have
// different ways of getting the name of a type.

#ifndef UTIL_TYPES_NAME_HPP
#define UTIL_TYPES_NAME_HPP

#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_format.h"

#if defined(__clang__) || defined(__GNUC__)
#include <cxxabi.h>
#endif

namespace util {
namespace internal {
inline std::string ComputeTypeName(std::type_info const& type_info) {
  auto* raw_name = type_info.name();
#if defined(__clang__) || defined(__GNUC__)
  int status;
  std::size_t length;
  auto* demangled_name =
      abi::__cxa_demangle(raw_name, nullptr, &length, &status);
  auto cleanup = absl::Cleanup([&] {
    if (demangled_name) {
      std::free(demangled_name);
    }
  });

  if (status == 0) {
    return std::string(demangled_name);
  }

  if (status == -2) {
    return absl::StrFormat("%s (invalid mangled)", raw_name);
  }

  throw std::runtime_error(
      absl::StrFormat("Failed to demangle type name. Status: %d", status));
#else
  return raw_name;
#endif
}

}  // namespace internal
template <class T>
std::string_view TypeName() {
  static const std::string name = internal::ComputeTypeName(typeid(T));
  return name;
}
}  // namespace util

#endif