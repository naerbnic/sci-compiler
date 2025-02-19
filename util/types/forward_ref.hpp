#ifndef REFERENCE_HPP
#define REFERENCE_HPP

#include <stdexcept>
#include <variant>
#include <vector>

#include "absl/functional/any_invocable.h"

// An object which is used to gather forward references to a value.
// Clients can add lambdas to this value to register callbacks which
// will be called when the value is resolved. If the value has already
// been resolved, the callback will be called immediately.
template <class ResolveType>
class ForwardRef {
 public:
  using ResolveFunc = absl::AnyInvocable<void(ResolveType const&) &&>;

  ForwardRef() : value_(std::vector<ResolveFunc>()) {}
  explicit ForwardRef(ResolveType&& value) : value_(std::move(value)) {}

  ForwardRef(ForwardRef const&) = delete;
  ForwardRef& operator=(ForwardRef const&) = delete;

  ForwardRef(ForwardRef&&) = default;
  ForwardRef& operator=(ForwardRef&&) = default;

  bool IsResolved() const {
    return std::holds_alternative<ResolveType>(value_);
  }

  void RegisterCallback(ResolveFunc func) {
    if (std::holds_alternative<ResolveType>(value_)) {
      std::move(func)(std::get<ResolveType>(value_));
    } else {
      std::get<std::vector<ResolveFunc>>(value_).push_back(std::move(func));
    }
  }

  void Resolve(ResolveType value) {
    if (std::holds_alternative<ResolveType>(value_)) {
      if (std::get<ResolveType>(value_) != value) {
        throw std::runtime_error(
            "ForwardReference already resolved with different value.");
      }
      return;
    }

    for (auto& func : std::get<std::vector<ResolveFunc>>(value_)) {
      std::move(func)(std::move(value));
    }

    value_ = std::move(value);
  }

  void Clear() { value_ = std::vector<ResolveFunc>(); }

 private:
  std::variant<std::vector<ResolveFunc>, ResolveType> value_;
};

#endif