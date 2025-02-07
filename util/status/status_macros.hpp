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

#define STATUS_MACROS_CONCAT_INNER_(x, y) x##y
#define STATUS_MACROS_CONCAT_(x, y) STATUS_MACROS_CONCAT_INNER_(x, y)

#define RETURN_IF_ERROR(expr)        \
  do {                               \
    const auto status = (expr);      \
    if (!status.ok()) return status; \
  } while (0)

#define ASSIGN_OR_RETURN_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                          \
  if (!statusor.ok()) return statusor.status();     \
  lhs = std::move(statusor.value())

#define ASSIGN_OR_RETURN(lhs, rexpr)                                       \
  ASSIGN_OR_RETURN_IMPL(STATUS_MACROS_CONCAT_(_status_or_value, __LINE__), \
                        lhs, rexpr)

#endif