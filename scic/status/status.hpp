#ifndef STATUS_STATUS_HPP
#define STATUS_STATUS_HPP

#include <string_view>

#include "absl/status/status.h"
#include "util/status/result.hpp"

namespace status {

using Status = absl::Status;

template <class T>
using StatusOr = util::Result<T, Status>;

inline Status OkStatus() { return absl::OkStatus(); }
inline Status NotFoundError(std::string_view message) {
  return absl::NotFoundError(message);
}
inline Status FailedPreconditionError(std::string_view message) {
  return absl::FailedPreconditionError(message);
}
inline Status UnimplementedError(std::string_view message) {
  return absl::UnimplementedError(message);
}
inline Status InvalidArgumentError(std::string_view message) {
  return absl::InvalidArgumentError(message);
}

inline bool IsNotFound(Status const& status) {
  return absl::IsNotFound(status);
}

using StatusCode = absl::StatusCode;

}  // namespace status

#endif