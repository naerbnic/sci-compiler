// Types for parser combinators.

#ifndef PARSERS_COMBINATORS_COMBINATORS_HPP
#define PARSERS_COMBINATORS_COMBINATORS_HPP

#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"

namespace parsers {

namespace internal {

template <class T>
std::vector<T> ConcatVectors(std::vector<T> a, std::vector<T> b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

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

}  // namespace internal

class ParseStatus {
 public:
  enum Kind {
    OK,
    FAILURE,
    FATAL,
  };

  static ParseStatus Ok() { return ParseStatus(OK, {}); }

  static ParseStatus Failure(std::vector<diag::Diagnostic> errors) {
    return ParseStatus(FAILURE, std::move(errors));
  }
  static ParseStatus Fatal(std::vector<diag::Diagnostic> errors) {
    return ParseStatus(FATAL, std::move(errors));
  }

  // Compose two errors together.
  ParseStatus operator|(ParseStatus const& other) const {
    if (kind_ == OK) {
      return other;
    }
    if (other.kind_ == OK) {
      return *this;
    }
    if (kind_ == other.kind_) {
      std::vector<diag::Diagnostic> messages = messages_;
      messages.insert(messages.end(), other.messages_.begin(),
                      other.messages_.end());
      return ParseStatus(kind_,
                         internal::ConcatVectors(messages_, other.messages_));
    }
    if (kind_ == FATAL) {
      return *this;
    }
    if (other.kind_ == FATAL) {
      return other;
    }
    throw std::runtime_error("Invalid error combination.");
  }

  ParseStatus() = default;

  Kind kind() const { return kind_; }
  absl::Span<diag::Diagnostic const> messages() const { return messages_; }
  bool ok() const { return kind_ == OK; }

 private:
  template <template <class...> class Type, class... Values>
  friend class ParseResultBase;

  ParseStatus PrependDiagnostics(
      std::vector<diag::Diagnostic> messages) const& {
    return ParseStatus(kind_,
                       internal::ConcatVectors(std::move(messages), messages_));
  }

  ParseStatus&& PrependDiagnostics(std::vector<diag::Diagnostic> messages) && {
    messages_ = internal::ConcatVectors(std::move(messages), messages_);
    return std::move(*this);
  }

  ParseStatus AppendDiagnostics(std::vector<diag::Diagnostic> messages) const& {
    return ParseStatus(kind_,
                       internal::ConcatVectors(messages_, std::move(messages)));
  }

  ParseStatus&& AppendDiagnostics(std::vector<diag::Diagnostic> messages) && {
    messages_ = internal::ConcatVectors(messages_, std::move(messages));
    return std::move(*this);
  }

  ParseStatus(Kind kind, std::vector<diag::Diagnostic> messages)
      : kind_(kind), messages_(std::move(messages)) {}

  Kind kind_ = OK;
  std::vector<diag::Diagnostic> messages_;
};

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

  ParseResult(Value value) : Base(std::move(value)) {}
  ParseResult(ParseStatus error) : Base(std::move(error)) {}

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

template <class Stream>
concept Streamable = requires(Stream& s) {
  typename Stream::elem_type;
  requires std::is_default_constructible_v<Stream>;
  requires std::copy_constructible<Stream>;
  requires std::is_copy_assignable_v<Stream>;
  { *s } -> std::convertible_to<typename Stream::elem_type const&>;
  { ++s } -> std::convertible_to<Stream&>;
  { s++ } -> std::convertible_to<Stream>;
  { bool(s) } -> std::same_as<bool>;
};

template <class Elem>
class ElemStream {
 public:
  using elem_type = Elem;

  ElemStream() = default;

  template <class T>
    requires(!std::same_as<std::decay_t<T>, ElemStream> && Streamable<T>)
  ElemStream(T&& t) : impl_(std::make_unique<Impl<T>>(std::forward<T>(t))) {}

  ElemStream(ElemStream const& other) : impl_(other.impl_->Clone()) {}
  ElemStream(ElemStream&&) = default;

  ElemStream& operator=(ElemStream const& other) {
    impl_ = other.impl_->Clone();
    return *this;
  }
  ElemStream& operator=(ElemStream&&) = default;

  Elem const& operator*() { return impl_->Current(); }
  Elem const* operator->() { return &impl_->Current(); }

  explicit operator bool() const { return !impl_->AtEnd(); }

  ElemStream& operator++() {
    impl_->Advance();
    return *this;
  }

  ElemStream operator++(int) {
    auto copy = *this;
    impl_->Advance();
    return copy;
  }

 private:
  class ImplBase {
   public:
    virtual ~ImplBase() = default;

    virtual Elem const& Current() = 0;
    virtual void Advance() = 0;
    virtual bool AtEnd() const = 0;
    virtual std::unique_ptr<ImplBase> Clone() const = 0;
  };

  template <class T>
  class Impl : public ImplBase {
   public:
    explicit Impl(T const& t) : t_(t) {}
    explicit Impl(T&& t) : t_(std::move(t)) {}

    Elem const& Current() override { return *t_; }
    void Advance() override { ++t_; }
    bool AtEnd() const override { return !bool(t_); }
    std::unique_ptr<ImplBase> Clone() const override {
      return std::make_unique<Impl>(t_);
    }

   private:
    T t_;
  };

  std::unique_ptr<ImplBase> impl_;
};

template <class Stream>
  requires Streamable<Stream>
ElemStream(Stream&&) -> ElemStream<typename Stream::elem_type>;

template <class Parser, class Elem, class = void>
struct ParserTraits {
  using result_type = std::invoke_result_t<Parser const&, ElemStream<Elem>&>;
};

template <class Parser, class Elem>
struct ParserTraits<Parser, Elem, typename Parser::template result_type<Elem>> {
  using result_type = typename Parser::template result_type<Elem>;
};

// Basic span element stream.

template <class Elem>
class SpanStream {
 public:
  using elem_type = Elem;

  SpanStream() = default;
  SpanStream(absl::Span<Elem const> span) : span_(span) {}

  Elem const& operator*() { return span_.front(); }
  Elem const* operator->() { return &span_.front(); }

  explicit operator bool() const { return !span_.empty(); }

  SpanStream& operator++() {
    span_.remove_prefix(1);
    return *this;
  }

  SpanStream operator++(int) {
    auto copy = *this;
    ++*this;
    return copy;
  }

 private:
  absl::Span<Elem const> span_;
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

  ParseResult<T> operator()(Args... args) const {
    if (!impl_) {
      return ParseResult<T>::OfFailure(
          diag::Diagnostic::Error("Uninitialized."));
    }
    return impl_->Parse(std::move(args)...);
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

  template <class F>
  ParseFunc(F f) : ParserFuncBase<R, Args...>(std::move(f)) {}
};

template <class F>
ParseFunc(F&& func)
    -> ParseFunc<typename internal::CallableTraits<F>::FunctionType>;

}  // namespace parsers

#endif