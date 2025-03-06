// Macros to help with error handling and status propagation.
//
// This is derived and inspired by the macro implementations in
// the Google Protobuf libraries:
//
// https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/stubs/status_macros.h
//
// This is under the BSD License. See the above link for the license.

#ifndef UTIL_STATUS_STATUS_MACROS_HPP_
#define UTIL_STATUS_STATUS_MACROS_HPP_

#include <concepts>
#include <source_location>
#include <type_traits>

namespace util::internal {
template <class T>
std::decay_t<T> WithLocationHelper(
    T&& status,
    std::source_location location = std::source_location::current()) {
  if constexpr (requires(T const& status) {
                  { status.WithLocation(location) } -> std::convertible_to<T>;
                }) {
    return std::forward<T>(status).WithLocation(location);
  } else {
    return std::forward<T>(status);
  }
}
}  // namespace util::internal

#define STATUS_MACROS_CONCAT_INNER_(x, y) x##y
#define STATUS_MACROS_CONCAT_(x, y) STATUS_MACROS_CONCAT_INNER_(x, y)

#define RETURN_IF_ERROR(expr)                                         \
  do {                                                                \
    const auto status = (expr);                                       \
    if (!status.ok())                                                 \
      return ::util::internal::WithLocationHelper(std::move(status)); \
  } while (0)

#define ASSIGN_OR_RETURN_IMPL(statusor, lhs, rexpr)                            \
  auto statusor = (rexpr);                                                     \
  if (!statusor.ok())                                                          \
    return ::util::internal::WithLocationHelper(std::move(statusor).status()); \
  lhs = std::move(statusor).value()

#define ASSIGN_OR_RETURN(lhs, rexpr)                                       \
  ASSIGN_OR_RETURN_IMPL(STATUS_MACROS_CONCAT_(_status_or_value, __LINE__), \
                        lhs, rexpr)

#endif