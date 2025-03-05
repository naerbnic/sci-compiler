#ifndef STATUS_STATUS_HPP
#define STATUS_STATUS_HPP

#include <memory>
#include <ostream>
#include <ranges>
#include <source_location>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "util/status/result.hpp"

namespace status {

class Status {
 public:
  Status() = default;
  Status(absl::Status status) : status_(std::move(status)) {}
  Status(Status const&) = default;
  Status(Status&&) = default;
  Status& operator=(Status const&) = default;
  Status& operator=(Status&&) = default;

  bool ok() const { return status_.ok(); }

  std::string_view message() const { return status_.message(); }

  Status WithLocation(
      std::source_location location = std::source_location::current()) const {
    auto new_payload = CopyPayload();
    new_payload.locations_.push_back(std::move(location));
    return Status(status_, std::make_shared<Payload>(std::move(new_payload)));
  }

 private:
  struct Payload {
    std::vector<std::source_location> locations_;
  };

  Status(absl::Status status, std::shared_ptr<Payload const> payload)
      : status_(std::move(status)), payload_(std::move(payload)) {}

  Payload CopyPayload() const {
    if (!payload_) {
      return {};
    }
    return *payload_;
  }

  Payload const& GetPayload() const {
    if (!payload_) {
      static Payload* empty_payload = new Payload;
      return *empty_payload;
    }
    return *payload_;
  }

  absl::Status status_;
  std::shared_ptr<Payload const> payload_;

  friend bool IsNotFound(Status const& status);

  friend std::ostream& operator<<(std::ostream& os, Status const& status) {
    os << status.status_;
    auto const& payload = status.GetPayload();
    if (!payload.locations_.empty()) {
      os << "\n== Source Locations ==\n";
      for (auto const& location :
           std::views::reverse(status.payload_->locations_)) {
        os << "- " << location.file_name() << ":" << location.line() << "\n";
      }
    }
    return os;
  }

  template <class Sink>
  friend void AbslStringify(Sink& sink, Status const& status) {
    absl::Format(&sink, "%v", status.status_);
    auto const& payload = status.GetPayload();
    if (!payload.locations_.empty()) {
      absl::Format(&sink, "\n== Source Locations ==\n");
      for (auto const& location :
           std::views::reverse(status.payload_->locations_)) {
        absl::Format(&sink, "- %s:%d\n", location.file_name(), location.line());
      }
    }
  }
};

template <class T>
using StatusOr = util::Result<T, Status>;

inline Status OkStatus() { return absl::OkStatus(); }
inline Status NotFoundError(
    std::string_view message,
    std::source_location location = std::source_location::current()) {
  return Status(absl::NotFoundError(message)).WithLocation(location);
}
inline Status FailedPreconditionError(
    std::string_view message,
    std::source_location location = std::source_location::current()) {
  return Status(absl::FailedPreconditionError(message)).WithLocation(location);
}
inline Status UnimplementedError(
    std::string_view message,
    std::source_location location = std::source_location::current()) {
  return Status(absl::UnimplementedError(message)).WithLocation(location);
}
inline Status InvalidArgumentError(
    std::string_view message,
    std::source_location location = std::source_location::current()) {
  return Status(absl::InvalidArgumentError(message)).WithLocation(location);
}

inline bool IsNotFound(Status const& status) {
  return absl::IsNotFound(status.status_);
}
}  // namespace status

#endif