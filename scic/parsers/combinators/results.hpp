#ifndef PARSERS_COMBINATORS_RESULTS_HPP
#define PARSERS_COMBINATORS_RESULTS_HPP

#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/internal_util.hpp"
#include "scic/parsers/combinators/status.hpp"
#include "util/types/tmpl.hpp"

namespace parsers {

namespace internal {

template <template <class...> class Result, class T>
struct WrapParseResultBaseImpl {
  using type = Result<T>;
  using elem = T;
};

template <template <class...> class Result, class T>
struct WrapParseResultBaseImpl<Result, Result<T>> {
  using type = Result<T>;
  using elem = T;
};

template <template <class...> class Result, class T>
using WrapParseResultBase = typename WrapParseResultBaseImpl<Result, T>::type;

// Marker trait, indicating that an internal constructor should be used
// in the case of variadic templates.
struct InnerCtorT {};
static constexpr InnerCtorT InnerCtor = InnerCtorT{};

}  // namespace internal

template <template <class...> class Type, class... Values>
class ParseResultBase {
 public:
  ParseResultBase(Values... values)
      : ParseResultBase(
            internal::InnerCtor,
            Success{
                .value = std::tuple<Values...>(std::move(values)...),
            }) {}

  ParseResultBase(ParseStatus error) : value_(error) {
    if (error.kind() == ParseStatus::OK) {
      throw std::runtime_error("Cannot create an OK value with ParseResult.");
    }
  }

  bool ok() const { return std::holds_alternative<Success>(value_); }
  std::tuple<Values...> const& values() const& { return success().value; }
  std::tuple<Values...>&& values() && {
    return std::move(*this).success().value;
  }
  ParseStatus const& status() const& { return std::get<ParseStatus>(value_); }
  ParseStatus&& status() && { return std::get<ParseStatus>(std::move(value_)); }

  // Inverts the result, turning a success into a failure, and a failure into a
  // success. A fatal error remains a fatal error.
  Type<> Invert() const {
    if (std::holds_alternative<Success>(value_)) {
      return Type<>(ParseStatus::Failure({}));
    } else if (std::holds_alternative<ParseStatus>(value_)) {
      auto const& error = std::get<ParseStatus>(value_);
      if (error.kind() == ParseStatus::FAILURE) {
        return Type<>();
      }
      return ParseResult<>(error);
    }
  }

  // Composes two parse results together, concatenating the values if both are
  // successful, or merging the errors if either is an error.
  template <class... MoreValues>
  Type<Values..., MoreValues...> operator|(Type<MoreValues...> other) const& {
    using CatResult = Type<Values..., MoreValues...>;
    if (ok() && other.ok()) {
      typename CatResult::Success success{
          .value = std::tuple_cat(std::move(std::move(*this).values()),
                                  std::move(std::move(other).values())),
      };
      return CatResult(internal::InnerCtor, std::move(success));
    }

    if (!ok() && !other.ok()) {
      return CatResult(std::move(*this).status() | std::move(other).status());
    }

    if (ok()) {
      return CatResult(std::move(other).status());
    }
    return CatResult(std::move(*this).status());
  }

  template <class... MoreValues>
  Type<Values..., MoreValues...> operator|(Type<MoreValues...> other) && {
    using CatResult = Type<Values..., MoreValues...>;
    if (ok() && other.ok()) {
      typename CatResult::Success success{
          .value = std::tuple_cat(std::move(std::move(*this).values()),
                                  std::move(std::move(other).values())),
      };
      return CatResult(internal::InnerCtor, std::move(success));
    }

    if (!ok() && !other.ok()) {
      return CatResult(std::move(*this).status() | std::move(other).status());
    }

    if (ok()) {
      return CatResult(std::move(other).status());
    }
    return CatResult(std::move(*this).status());
  }

  template <class F>
  auto Apply(F&& f) && {
    using Result =
        internal::WrapParseResultBase<Type,
                                      std::invoke_result_t<F&&, Values&&...>>;
    if (ok()) {
      return Result(std::apply(std::forward<F>(f), std::move(*this).values()));
    }
    return Result(std::move(*this).status());
  }

  template <class F>
  auto Apply(F&& f) const& {
    using Result =
        internal::WrapParseResultBase<Type,
                                      std::invoke_result_t<F&&, Values&&...>>;
    if (ok()) {
      return Result(std::apply(std::forward<F>(f), std::move(*this).values()));
    }
    return Result(std::move(*this).status());
  }

 protected:
  template <template <class...> class OtherType, class... OtherValues>
  friend class ParseResultBase;
  // This follows the general idea in Rust's nom, with a difference between a
  // failure, which can be recovered from, and an error, which is a hard stop.
  struct Success {
    // The value this carries.
    std::tuple<Values...> value;
  };

  Success const& success() const& { return std::get<Success>(value_); }
  Success& success() & { return std::get<Success>(value_); }
  Success&& success() && { return std::get<Success>(std::move(value_)); }

  void PrependMessages(std::vector<diag::Diagnostic> messages) {
    if (ok()) {
      auto& success_value = success();
      success_value.non_errors = internal::ConcatVectors(
          std::move(messages), std::move(success_value.non_errors));
    }
  }

  using ResultValue = std::variant<Success, ParseStatus>;

  explicit ParseResultBase(internal::InnerCtorT const&, ResultValue value)
      : value_(std::move(value)) {}

 private:
  ResultValue value_;
};

template <class... Values>
class ParseResult : public ParseResultBase<ParseResult, Values...> {
  using Base = ParseResultBase<ParseResult, Values...>;

 public:
  static ParseResult OfFailure(diag::Diagnostic error) {
    return ParseResult(ParseStatus::Failure({std::move(error)}));
  }

  static ParseResult OfError(diag::Diagnostic error) {
    return ParseResult(ParseStatus::Fatal({std::move(error)}));
  }

  ParseResult(Values&&... values) : Base(std::forward<Values>(values)...) {}
  ParseResult(ParseStatus error) : Base(std::move(error)) {}

 private:
  template <template <class...> class OtherType, class... OtherValues>
  friend class ParseResultBase;

  ParseResult(internal::InnerCtorT const&, typename Base::ResultValue value)
      : Base(internal::InnerCtor, std::move(value)) {}
};

template <class Value>
class ParseResult<Value> : public ParseResultBase<ParseResult, Value> {
  using Base = ParseResultBase<ParseResult, Value>;

 public:
  static ParseResult OfFailure(diag::Diagnostic error) {
    return ParseResult(ParseStatus::Failure({std::move(error)}));
  }

  static ParseResult OfError(diag::Diagnostic error) {
    return ParseResult(ParseStatus::Fatal({std::move(error)}));
  }

  template <class U>
    requires(std::constructible_from<Value, U &&> &&
             !util::TemplateTraits<ParseResult>::IsSpecialization<U>)
  ParseResult(U&& value) : Base(Value(std::forward<U>(value))) {}
  ParseResult(ParseStatus error) : Base(std::move(error)) {}

  template <class U>
    requires(!std::same_as<Value, U> && std::constructible_from<U, Value>)
  operator ParseResult<U>() && {
    if (this->ok()) {
      return ParseResult<U>(std::move(*this).value());
    } else {
      return ParseResult<U>(std::move(*this).status());
    }
  }

  Value const& value() const& { return std::get<0>(Base::values()); }
  Value&& value() && { return std::get<0>(std::move(*this).Base::values()); }

 private:
  template <template <class...> class OtherType, class... OtherValues>
  friend class ParseResultBase;

  ParseResult(typename Base::ResultValue value) : Base(std::move(value)) {}
};

template <class... Values>
ParseResult(Values&&...) -> ParseResult<std::decay_t<Values>...>;

template <class T>
using WrapParseResult = internal::WrapParseResultBase<ParseResult, T>;

template <class T>
using ExtractParseResult =
    typename internal::WrapParseResultBaseImpl<ParseResult, T>::elem;

template <class F, class... Args>
using ParseResultOf = WrapParseResult<std::invoke_result_t<F, Args...>>;

template <class F, class... Args>
using ParseResultElemOf = ExtractParseResult<std::invoke_result_t<F, Args...>>;

}  // namespace parsers

#endif