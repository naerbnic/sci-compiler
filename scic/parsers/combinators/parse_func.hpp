#ifndef PARSERS_COMBINATORS_PARSE_FUNC_HPP
#define PARSERS_COMBINATORS_PARSE_FUNC_HPP

#include <memory>
#include <tuple>
#include <type_traits>

#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/results.hpp"

namespace parsers {

namespace internal {

// If we have a type that has a single operator(), extract the args and return
// type.

template <class T>
struct MemberFunctionTraitsImpl;

template <class Obj, class R, class... Args>
struct MemberFunctionTraitsImpl<R (Obj::*)(Args...) const> {
  using ReturnType = R;
  using ArgsTuple = std::tuple<Args...>;
  using FunctionType = R(Args...);
};

template <class T>
struct MemberFunctionTraits : public MemberFunctionTraitsImpl<T> {};

template <class T>
struct CallableTraitsBase {
 private:
  using MemberTraits = MemberFunctionTraits<decltype(&T::operator())>;

 public:
  using ReturnType = typename MemberTraits::ReturnType;
  using ArgsTuple = typename MemberTraits::ArgsTuple;
  using FunctionType = typename MemberTraits::FunctionType;
};

// Function types
template <class R, class... Args>
struct CallableTraitsBase<R (*)(Args...)> {
  using ReturnType = R;
  using ArgsTuple = std::tuple<Args...>;
  using FunctionType = R(Args...);
};

template <class T>
using CallableTraits = CallableTraitsBase<std::decay_t<T>>;

template <class T>
struct SimplifyParseReturnImpl;

template <class R, class... Args>
struct SimplifyParseReturnImpl<R(Args...)> {
  using type = ExtractParseResult<R>(Args...);
};

template <class T>
using SimplifyParseReturn = typename SimplifyParseReturnImpl<T>::type;

}  // namespace internal

template <class T>
struct ParserTraits {
 private:
  using Traits = internal::CallableTraits<T>;

 public:
  using RawFuncT = Traits::FunctionType;
  using BaseFuncT =
      internal::SimplifyParseReturn<typename Traits::FunctionType>;
  using ParseRet = WrapParseResult<typename Traits::ReturnType>;
  using BaseRet = ExtractParseResult<typename Traits::ReturnType>;
  using ArgsTuple = typename Traits::ArgsTuple;
};

template <class T>
concept IsParser = requires() { typename ParserTraits<T>; };

template <class T, class... Args>
concept IsParserOf = requires() {
  requires std::is_move_constructible_v<T>;
  typename ParserTraits<T>;
  requires std::convertible_to<typename ParserTraits<T>::ArgsTuple,
                               std::tuple<Args...>>;
};

template <class T, class... Args>
class ParserFuncBase {
 public:
  ParserFuncBase() = default;
  ParserFuncBase(ParserFuncBase const&) = default;
  ParserFuncBase(ParserFuncBase&&) = default;
  ParserFuncBase& operator=(ParserFuncBase const&) = default;
  ParserFuncBase& operator=(ParserFuncBase&&) = default;

  template <class F>
    requires(!std::same_as<std::decay_t<F>, ParserFuncBase> &&
             std::invocable<F const&, Args...> &&
             std::convertible_to<std::invoke_result_t<F const&, Args...>,
                                 ParseResult<T>>)
  ParserFuncBase(F f) : impl_(std::make_shared<Impl<F>>(std::move(f))) {}

  ParseResult<T> operator()(Args&&... args) const {
    if (!impl_) {
      return ParseResult<T>::OfFailure(
          diag::Diagnostic::Error("Uninitialized."));
    }
    return impl_->Parse(std::forward<Args>(args)...);
  }

 private:
  class ImplBase {
   public:
    virtual ~ImplBase() = default;

    virtual ParseResult<T> Parse(Args...) const = 0;
  };

  template <class F>
  class Impl : public ImplBase {
   public:
    explicit Impl(F f) : func_(std::move(f)) {}

    ParseResult<T> Parse(Args... args) const override { return func_(args...); }

   private:
    F func_;
  };

  std::shared_ptr<ImplBase> impl_;
};

template <class T>
struct ParseFunc;

template <class R, class... Args>
struct ParseFunc<R(Args...)> : public ParserFuncBase<R, Args...> {
 public:
  ParseFunc() = default;
  ParseFunc(ParseFunc const&) = default;
  ParseFunc(ParseFunc&&) = default;
  ParseFunc& operator=(ParseFunc const&) = default;
  ParseFunc& operator=(ParseFunc&&) = default;

  template <class F>
  ParseFunc(F f) : ParserFuncBase<R, Args...>(std::move(f)) {}
};

template <class F>
ParseFunc(F&& func) -> ParseFunc<internal::SimplifyParseReturn<
    typename internal::CallableTraits<F>::FunctionType>>;
}  // namespace parsers
#endif