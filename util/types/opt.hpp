#ifndef UTIL_TYPES_OPT_HPP
#define UTIL_TYPES_OPT_HPP

#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>

#include "util/types/any_storage.hpp"
#include "util/types/tmpl.hpp"

namespace util {

template <class T>
class Opt {
  static_assert(!TemplateTraits<Opt>::IsSpecialization<T>,
                "The type parameter of \"Opt\" cannot be an \"Opt\".");

 public:
  using value_type = T;

  constexpr Opt() noexcept = default;
  constexpr Opt(std::nullopt_t nullopt) noexcept : storage_(nullopt) {}

  constexpr Opt(Opt const& other) = default;
  constexpr Opt(Opt&& other) noexcept = default;
  constexpr Opt& operator=(Opt const& other) = default;
  constexpr Opt& operator=(Opt&& other) noexcept = default;

  template <class U>
    requires std::constructible_from<T, const U&>
  constexpr Opt(Opt<U> const& other) : storage_(other.storage_) {}

  template <class U>
    requires std::constructible_from<T, U>
  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
  constexpr Opt(Opt<U>&& other) : storage_(std::move(other.storage_)) {}

  template <class... Args>
    requires std::constructible_from<T, Args...>
  constexpr explicit Opt(std::in_place_t in_place, Args&&... args)
      : storage_(in_place, in_place, std::forward<Args>(args)...) {}
  template <class U, class... Args>
    requires std::constructible_from<T, std::initializer_list<U>&, Args...>
  constexpr explicit Opt(std::in_place_t in_place,
                         std::initializer_list<U> ilist, Args&&... args)
      : storage_(in_place, in_place, ilist, std::forward<Args>(args)...) {}

  template <class U = std::remove_cv_t<T>>
    requires(std::constructible_from<T, U&> &&
             !std::same_as<std::remove_cv_t<U>, std::in_place_t> &&
             !std::same_as<std::remove_cv_t<U>, Opt<U>> &&
             !TemplateTraits<Opt>::IsSpecialization<U>)
  constexpr Opt(U&& value) : storage_(AnyStorage<T>(std::forward<U>(value))) {}

  constexpr AnyStorage<T>::ConstPtrType operator->() const noexcept {
    return &storage_->value();
  }

  constexpr AnyStorage<T>::PtrType operator->() noexcept {
    return &storage_->value();
  }

  constexpr AnyStorage<T>::ConstRefType operator*() const& noexcept {
    return storage_->value();
  }

  constexpr AnyStorage<T>::RefType operator*() & noexcept {
    return storage_->value();
  }

  constexpr AnyStorage<T>::ConstMoveType operator*() const&& noexcept {
    return storage_->value();
  }

  constexpr AnyStorage<T>::MoveType operator*() && noexcept {
    return storage_->value();
  }

  constexpr explicit operator bool() const noexcept {
    return storage_.has_value();
  }
  constexpr bool has_value() const noexcept { return storage_.has_value(); }

  constexpr AnyStorage<T>::ConstRefType value() const& {
    return storage_->value();
  }
  constexpr AnyStorage<T>::RefType value() & { return storage_->value(); }
  constexpr AnyStorage<T>::ConstMoveType value() const&& {
    return storage_->value();
  }
  constexpr AnyStorage<T>::MoveType value() && {
    return std::move(storage_).value();
  }
  template <class U = std::remove_cv_t<T>>
  constexpr AnyStorage<T>::ConstType value_or(U&& default_value) const& {
    if (storage_.has_value()) {
      return storage_->value();
    } else {
      return std::forward<U>(default_value);
    }
  }
  template <class U = std::remove_cv_t<T>>
  constexpr AnyStorage<T>::Type value_or(U&& default_value) && {
    if (storage_.has_value()) {
      return storage_->value();
    } else {
      return std::forward<U>(default_value);
    }
  }

  void swap(Opt& other) noexcept { storage_.swap(other.storage_); }
  void reset() noexcept { storage_.reset(); }
  template <class... Args>
    requires std::constructible_from<T, Args...>
  AnyStorage<T>::RefType emplace(Args&&... args) {
    storage_.emplace(std::in_place, std::forward<Args>(args)...);
    return storage_->value();
  }

  template <class U, class... Args>
    requires std::constructible_from<T, std::initializer_list<U>&, Args&&...>
  AnyStorage<T>::RefType emplace(std::initializer_list<U> ilist,
                                 Args&&... args) {
    storage_.emplace(std::in_place, std::move(ilist),
                     std::forward<Args>(args)...);
    return storage_->value();
  }

  template <std::equality_comparable_with<T> U>
  constexpr bool operator==(const Opt<U>& rhs) const {
    if (!storage_.has_value() && !rhs.storage_.has_value()) {
      return true;
    } else if (!storage_.has_value() || !rhs.storage_.has_value()) {
      return false;
    }
    return (storage_->value() == rhs.storage_->value());
  }

  template <std::three_way_comparable_with<T> U>
  constexpr std::compare_three_way_result_t<T, U> operator<=>(
      const Opt<U>& rhs) const {
    if (!storage_.has_value() && !rhs.storage_.has_value()) {
      return std::strong_ordering::equal;
    } else if (!storage_.has_value()) {
      return std::strong_ordering::less;
    } else if (!rhs.storage_.has_value()) {
      return std::strong_ordering::greater;
    }
    return (storage_.value() <=> rhs.storage_.value());
  }

  constexpr bool operator==(std::nullopt_t) const {
    return (!storage_.has_value());
  }

  constexpr std::strong_ordering operator<=>(std::nullopt_t) const {
    if (!storage_.has_value()) {
      return std::strong_ordering::equal;
    }
    return std::strong_ordering::greater;
  }

  template <class U>
    requires(!TemplateTraits<Opt>::IsSpecialization<U> &&
             std::equality_comparable_with<U, T>)
  constexpr bool operator==(const U& value) const {
    if (!storage_.has_value()) {
      return false;
    }
    return (storage_->value() == value);
  }

  template <class U>
    requires(!TemplateTraits<Opt>::IsSpecialization<U> &&
             std::three_way_comparable_with<U, T>)
  constexpr std::compare_three_way_result_t<T, U> operator<=>(
      const U& value) const {
    if (!storage_.has_value()) {
      return std::strong_ordering::less;
    }
    return (storage_->value() <=> value);
  }

 private:
  std::optional<AnyStorage<T>> storage_;
};

}  // namespace util

// Hash implementation
template <class T>
struct std::hash<util::Opt<T>> {
  std::size_t operator()(const util::Opt<T>& opt) const {
    return std::hash<std::optional<T>>{}(opt.storage_);
  }
};

#endif