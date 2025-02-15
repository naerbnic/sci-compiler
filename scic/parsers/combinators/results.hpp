#ifndef PARSERS_COMBINATORS_RESULTS_HPP
#define PARSERS_COMBINATORS_RESULTS_HPP

#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/has_absl_stringify.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/status.hpp"
#include "util/types/tmpl.hpp"

namespace parsers {

namespace internal {

template <template <class> class Result, class T>
struct WrapParseResultBaseImpl {
  using type = Result<T>;
  using elem = T;
};

template <template <class> class Result, class T>
struct WrapParseResultBaseImpl<Result, Result<T>> {
  using type = Result<T>;
  using elem = T;
};

template <template <class> class Result, class T>
using WrapParseResultBase = typename WrapParseResultBaseImpl<Result, T>::type;

}  // namespace internal

template <class Value>
class ParseResult {
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
  ParseResult(U&& values)
      : ParseResult(Success{
            .value = std::forward<U>(values),
        }) {}

  ParseResult(ParseStatus error) : value_(error) {
    if (error.kind() == ParseStatus::OK) {
      throw std::runtime_error("Cannot create an OK value with ParseResult.");
    }
  }

  bool ok() const { return std::holds_alternative<Success>(value_); }
  Value const& value() const& { return success().value; }
  Value&& value() && { return std::move(*this).success().value; }
  ParseStatus const& status() const& { return std::get<ParseStatus>(value_); }
  ParseStatus&& status() && { return std::get<ParseStatus>(std::move(value_)); }

  template <class U>
    requires(!std::same_as<Value, U> && std::constructible_from<U, Value>)
  operator ParseResult<U>() && {
    if (this->ok()) {
      return ParseResult<U>(std::move(*this).value());
    } else {
      return ParseResult<U>(std::move(*this).status());
    }
  }

 private:
  // This follows the general idea in Rust's nom, with a difference between a
  // failure, which can be recovered from, and an error, which is a hard stop.
  struct Success {
    // The value this carries.
    Value value;
  };
  using ResultValue = std::variant<Success, ParseStatus>;

  ParseResult(Success value) : value_(std::move(value)) {}

  Success const& success() const& { return std::get<Success>(value_); }
  Success& success() & { return std::get<Success>(value_); }
  Success&& success() && { return std::get<Success>(std::move(value_)); }

  ResultValue value_;

  friend std::ostream& operator<<(std::ostream& os,
                                  ParseResult<Value> const& result)
    requires(requires(Value const& v, std::ostream& os) {
      { os << v } -> std::same_as<std::ostream&>;
    })
  {
    if (result.ok()) {
      return (os << result.value());
    } else {
      return (os << result.status());
    }
  }

  template <class Sink>
  friend void AbslStringify(Sink& os, ParseResult<Value> const& result)
    requires(absl::HasAbslStringify<Value>())
  {
    if (result.ok()) {
      return (os << result.value());
    } else {
      return (os << result.status());
    }
  }
};

template <class Value>
ParseResult(Value&&) -> ParseResult<std::decay_t<Value>>;

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