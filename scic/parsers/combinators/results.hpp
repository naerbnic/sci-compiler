#ifndef PARSERS_COMBINATORS_RESULTS_HPP
#define PARSERS_COMBINATORS_RESULTS_HPP

#include <type_traits>
#include <utility>
#include <vector>

#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/status.hpp"
#include "util/status/result.hpp"

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
class ParseResult : public util::Result<Value, ParseStatus> {
 public:
  static ParseResult OfFailure(diag::Diagnostic error) {
    return ParseResult(ParseStatus::Failure({std::move(error)}));
  }

  static ParseResult OfError(diag::Diagnostic error) {
    return ParseResult(ParseStatus::Fatal({std::move(error)}));
  }

  using util::Result<Value, ParseStatus>::Result;
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