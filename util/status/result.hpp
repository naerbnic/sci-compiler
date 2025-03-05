#ifndef UTIL_STATUS_RESULT_HPP
#define UTIL_STATUS_RESULT_HPP

#include <optional>
#include <ostream>
#include <type_traits>
#include <variant>

#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/str_format.h"
#include "util/types/tmpl.hpp"

namespace util {

template <class T>
concept StatusLike = requires(T const& t_ref) {
  { t_ref.ok() } -> std::convertible_to<bool>;
};

template <class T>
concept Mergable = requires(T&& t1, T&& t2) {
  { t1 | t2 } -> std::convertible_to<T>;
};

template <class T, class To, class... NotTo>
concept ConvertibleToExclusive =
    std::convertible_to<T, To> && (!std::convertible_to<T, NotTo> && ...);

template <class T, class Err>
class Result {
 public:
  using value_type = T;
  using err_type = Err;
  static Result ValueOf(T value) { return Result(Ok{std::move(value)}); }
  static Result ErrorOf(Err err) { return Result(Error{std::move(err)}); }

  // All of the typical default should be available, unless implicitly deleted.
  Result() = default;
  Result(Result const&) = default;
  Result(Result&&) = default;

  // Implicit conversions.

  // If we can distinguish between the value and error types, any type
  // that is convertible to only one of them can be used to construct a Result.
  template <class U>
    requires(!std::same_as<T, Err> && ConvertibleToExclusive<U, T, Err> &&
             !util::TemplateTraits<Result>::IsSpecialization<U>)
  Result(U&& value) : Result(Ok{std::forward<U>(value)}) {}

  template <class U>
    requires(!std::same_as<T, Err> && ConvertibleToExclusive<U, Err, T> &&
             !util::TemplateTraits<Result>::IsSpecialization<U>)
  Result(U&& err) : Result(Error{std::forward<U>(err)}) {}

  // If we get another Result, we can convert it to this one if the individual
  // types are convertible.
  template <class OtherT, class OtherErr>
    requires(std::convertible_to<OtherT, T> &&
             std::convertible_to<OtherErr, Err>)
  Result(Result<OtherT, OtherErr> other);

  bool ok() const { return std::holds_alternative<Ok>(value_); }

  T& value() & { return std::get<Ok>(value_).value; }
  T const& value() const& { return std::get<Ok>(value_).value; }
  T&& value() && { return std::move(std::get<Ok>(value_).value); }
  T const&& value() const&& { return std::move(std::get<Ok>(value_).value); }

  Err& status() & { return std::get<Error>(value_).value; }
  Err const& status() const& { return std::get<Error>(value_).value; }
  Err&& status() && { return std::move(std::get<Error>(value_).value); }
  Err const&& status() const&& {
    return std::move(std::get<Error>(value_).value);
  }

  // Pointer semantics.

  T* get() { return &value(); }
  T const* get() const { return &value(); }

  T* operator->() { return get(); }
  T const* operator->() const { return get(); }
  T& operator*() { return value(); }
  T const& operator*() const { return value(); }
  explicit operator bool() const { return ok(); }

  // Assignment Operators
  Result& operator=(Result const&) = default;
  Result& operator=(Result&&) = default;

  // Conversion assignment from another result.
  //
  // Includes implicit conversion from T.
  template <class U>
    requires(!std::same_as<T, Err> && ConvertibleToExclusive<U, T, Err> &&
             !util::TemplateTraits<Result>::IsSpecialization<U>)
  Result& operator=(U&& value) {
    value_ = Ok{std::forward<U>(value)};
    return *this;
  }

  // Conversion assignment from another status.
  //
  // Includes implicit conversion from T.
  template <class U>
    requires(!std::same_as<T, Err> && ConvertibleToExclusive<U, Err, T> &&
             !util::TemplateTraits<Result>::IsSpecialization<U>)
  Result& operator=(U&& err) {
    value_ = Error{std::forward<U>(err)};
    return *this;
  }

  template <class OtherT, class OtherErr>

    requires(std::convertible_to<OtherT, T> &&
             std::convertible_to<OtherErr, Err>)
  Result& operator=(Result<OtherT, OtherErr>&& other) {
    value_ = ConvertFrom(std::move(other));
    return *this;
  }

 private:
  struct Ok {
    T value;
  };
  struct Error {
    Err value;
  };

  using Value = std::variant<Ok, Error>;

  template <class OtherT, class OtherErr>
  Value ConvertFrom(Result<OtherT, OtherErr>&& other) {
    if (other.ok()) {
      return Ok{std::move(other).value()};
    } else {
      return Error{std::move(other).status()};
    }
  }

  Result(Value value) : value_(std::move(value)) {}
  Value value_;

  friend std::ostream& operator<<(std::ostream& os,
                                  Result<T, Err> const& result)
    requires(requires(T const& v, Err const& err, std::ostream& os) {
      { os << v } -> std::same_as<std::ostream&>;
      { os << err } -> std::same_as<std::ostream&>;
    })
  {
    if (result.ok()) {
      return (os << result.value());
    } else {
      return (os << result.status());
    }
  }

  template <class Sink>
  friend void AbslStringify(Sink& os, Result<T, Err> const& result)
    requires(absl::HasAbslStringify<T>() && absl::HasAbslStringify<Err>())
  {
    if (result.ok()) {
      absl::Format(&os, "%v", result.value());
    } else {
      absl::Format(&os, "%v", result.status());
    }
  }
};

template <class T, class Err>
template <class OtherT, class OtherErr>
  requires(std::convertible_to<OtherT, T> && std::convertible_to<OtherErr, Err>)
Result<T, Err>::Result(Result<OtherT, OtherErr> other)
    : Result(ConvertFrom(std::move(other))) {}

template <class F, class... Ts, class... Errs>
  requires std::invocable<F, Ts...>
auto ApplyResults(F&& body, Result<Ts, Errs>&&... values) {
  using ReturnValue = std::invoke_result_t<F, Ts...>;
  if constexpr (util::TemplateTraits<Result>::IsSpecialization<ReturnValue>) {
    // If this is a Result instance, then all of the errors should be
    // convertible to that error type.
    using ErrType = typename ReturnValue::err_type;
    static_assert((std::convertible_to<Errs, ErrType> && ...));

    std::optional<ErrType> min_err;
    (
        [&] {
          if (!values.ok()) {
            if (!min_err) {
              min_err = std::move(values).status();
            } else {
              if constexpr (Mergable<ErrType>) {
                // Merge the errors.
                min_err = std::move(*min_err) | std::move(values).status();
              } else {
                // Drop this error, and keep the previous one.
              }
            }
          }
        }(),
        ...);

    if (min_err) {
      return ReturnValue::ErrorOf(std::move(*min_err));
    } else {
      return std::invoke(std::forward<F>(body), std::move(values).value()...);
    }
  } else {
    // If the result of the body is not a Result, then the return type of
    // this function should be a result with the return type as the value,
    // and the minimum of the error types. We can just leverage a recursive
    // call, as the result type will be a specialization of Result.
    using FinalErr = std::common_reference_t<Errs...>;
    using ResultType = Result<ReturnValue, FinalErr>;
    return ApplyResults(
        [&body](Ts&&... values) -> ResultType {
          return std::invoke(std::forward<F>(body),
                             std::forward<Ts>(values)...);
        },
        std::forward<Result<Ts, Errs>>(values)...);
  }
}

}  // namespace util
#endif
