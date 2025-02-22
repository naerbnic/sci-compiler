#ifndef SEM_LATE_BOUND_HPP
#define SEM_LATE_BOUND_HPP

#include <optional>
#include <stdexcept>
namespace sem {

// Similar to an Optional, but is intended to only be updated once.
//
// This should be used as a field, as the mutators do not allow for arbitrary
// assignment, which would make it less useful for a parameter.
template <class T>
class LateBound {
 public:
  LateBound() = default;

  LateBound(LateBound const&) = delete;
  LateBound& operator=(LateBound const&) = delete;

  LateBound(LateBound&&) = default;
  LateBound& operator=(LateBound&&) = default;

  void set(T value) {
    if (value_.has_value()) {
      throw std::logic_error("LateBound value already set");
    }
    value_ = std::move(value);
  }

  bool has_value() const { return value_.has_value(); }

  // Pointer semantics
  T* operator->() { return &value_.value(); }
  T const* operator->() const { return &value_.value(); }
  T& operator*() { return value_.value(); }
  T const& operator*() const { return value_.value(); }

 private:
  std::optional<T> value_;
};

}  // namespace sem

#endif