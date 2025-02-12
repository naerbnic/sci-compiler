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

template <class... Values>
class ParseResult;

namespace internal {

template <class T>
std::vector<T> ConcatVectors(std::vector<T> a, std::vector<T> b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

template <template <class...> class Result, class T>
struct WrapParseResultImpl {
  using type = Result<T>;
};

template <template <class...> class Result, class T>
struct WrapParseResultImpl<Result, Result<T>> {
  using type = ParseResult<T>;
};

template <template <class...> class Result, class T>
using WrapParseResult = typename WrapParseResultImpl<Result, T>::type;

}  // namespace internal

class ParseError {
 public:
  enum Kind {
    EMPTY,
    FAILURE,
    FATAL,
  };

  static ParseError Failure(std::vector<diag::Diagnostic> errors) {
    return ParseError(FAILURE, std::move(errors));
  }
  static ParseError Fatal(std::vector<diag::Diagnostic> errors) {
    return ParseError(FATAL, std::move(errors));
  }

  // Compose two errors together.
  ParseError operator|(ParseError const& other) const {
    if (kind_ == EMPTY) {
      return other;
    }
    if (other.kind_ == EMPTY) {
      return *this;
    }
    if (kind_ == other.kind_) {
      std::vector<diag::Diagnostic> messages = messages_;
      messages.insert(messages.end(), other.messages_.begin(),
                      other.messages_.end());
      return ParseError(kind_,
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

  ParseError() = default;

  Kind kind() const { return kind_; }
  absl::Span<diag::Diagnostic const> messages() const { return messages_; }

 private:
  template <template <class...> class Type, class... Values>
  friend class ParseResultBase;

  ParseError PrependDiagnostics(std::vector<diag::Diagnostic> messages) const& {
    return ParseError(kind_,
                      internal::ConcatVectors(std::move(messages), messages_));
  }

  ParseError&& PrependDiagnostics(std::vector<diag::Diagnostic> messages) && {
    messages_ = internal::ConcatVectors(std::move(messages), messages_);
    return std::move(*this);
  }

  ParseError AppendDiagnostics(std::vector<diag::Diagnostic> messages) const& {
    return ParseError(kind_,
                      internal::ConcatVectors(messages_, std::move(messages)));
  }

  ParseError&& AppendDiagnostics(std::vector<diag::Diagnostic> messages) && {
    messages_ = internal::ConcatVectors(messages_, std::move(messages));
    return std::move(*this);
  }

  ParseError(Kind kind, std::vector<diag::Diagnostic> messages)
      : kind_(kind), messages_(std::move(messages)) {}

  Kind kind_ = EMPTY;
  std::vector<diag::Diagnostic> messages_;
};

template <template <class...> class Type, class... Values>
class ParseResultBase {
 public:
  ParseResultBase(Values&&... values)
      : ParseResultBase(Success{
            .value = std::tuple<Values...>(std::forward<Values>(values)...),
        }) {}

  ParseResultBase(ParseError error) : value_(error) {}

  Type<Values...>& AddWarning(diag::Diagnostic warning) {
    if (std::holds_alternative<Success>(value_)) {
      std::get<Success>(value_).non_errors_.push_back(std::move(warning));
    }
    return *this;
  }

  bool ok() const { return std::holds_alternative<Success>(value_); }
  std::tuple<Values...> const& values() const& { return success().value; }
  std::tuple<Values...>&& values() && {
    return std::move(*this).success().value;
  }
  ParseError const& status() const& { return std::get<ParseError>(value_); }
  ParseError&& status() && { return std::get<ParseError>(std::move(value_)); }

  // Inverts the result, turning a success into a failure, and a failure into a
  // success. A fatal error remains a fatal error.
  Type<> Invert() const {
    if (std::holds_alternative<Success>(value_)) {
      return Type<>(ParseError::Failure({}));
    } else if (std::holds_alternative<ParseError>(value_)) {
      auto const& error = std::get<ParseError>(value_);
      if (error.kind() == ParseError::FAILURE) {
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
      auto&& this_success = std::move(*this).success();
      auto&& other_success = std::move(other).success();
      typename CatResult::Success success{
          .value = std::tuple_cat(std::move(this_success.value),
                                  std::move(other_success.value)),
          .non_errors =
              internal::ConcatVectors(std::move(this_success.non_errors),
                                      std::move(other_success.non_errors)),
      };
      return CatResult(std::move(success));
    }

    if (!ok() && !other.ok()) {
      return CatResult(std::move(*this).status() | std::move(other).status());
    }

    if (ok()) {
      auto&& this_success = std::move(*this).success();
      auto&& other_error = std::move(other).status();
      ;
      return CatResult(
          std::move(other_error)
              .PrependDiagnostics(std::move(this_success.non_errors)));
    }
    auto&& this_error = std::move(*this).status();
    auto&& other_success = std::move(other).success();
    return CatResult(
        std::move(this_error)
            .AppendDiagnostics(std::move(other_success.non_errors)));
  }

  template <class... MoreValues>
  Type<Values..., MoreValues...> operator|(Type<MoreValues...> other) && {
    using CatResult = Type<Values..., MoreValues...>;
    if (ok() && other.ok()) {
      auto&& this_success = std::move(*this).success();
      auto&& other_success = std::move(other).success();
      typename CatResult::Success success{
          .value = std::tuple_cat(std::move(this_success.value),
                                  std::move(other_success.value)),
          .non_errors =
              internal::ConcatVectors(std::move(this_success.non_errors),
                                      std::move(other_success.non_errors)),
      };
      return CatResult(std::move(success));
    }

    if (!ok() && !other.ok()) {
      return CatResult(std::move(*this).status() | std::move(other).status());
    }

    if (ok()) {
      auto&& this_success = std::move(*this).success();
      auto&& other_error = std::move(other).status();
      ;
      return CatResult(
          std::move(other_error)
              .PrependDiagnostics(std::move(this_success.non_errors)));
    }
    auto&& this_error = std::move(*this).status();
    auto&& other_success = std::move(other).success();
    return CatResult(
        std::move(this_error)
            .AppendDiagnostics(std::move(other_success.non_errors)));
  }

  template <class F>
  auto Apply(F&& f) && {
    using Result =
        internal::WrapParseResult<Type, std::invoke_result_t<F&&, Values&&...>>;
    if (std::holds_alternative<Success>(value_)) {
      auto success = std::move(*this).success();
      auto result =
          Result(std::apply(std::forward<F>(f), std::move(success.value)));
      result.PrependMessages(std::move(success.non_errors));
      return result;
    }
    return Result{std::get<ParseError>(value_)};
  }

  template <class F>
  auto Apply(F&& f) const& -> internal::WrapParseResult<
      Type, std::invoke_result_t<F&&, Values const&...>> {
    using Result =
        internal::WrapParseResult<Type,
                                  std::invoke_result_t<F&&, Values const&...>>;
    if (std::holds_alternative<Success>(value_)) {
      auto& success = std::get<Success>(value_);
      auto result = Result(std::apply(std::forward<F>(f), success.value));
      result.PrependMessages(success.non_errors);
      return result;
    }
    return Result{std::get<ParseError>(value_)};
  }

 protected:
  template <template <class...> class OtherType, class... OtherValues>
  friend class ParseResultBase;
  // This follows the general idea in Rust's nom, with a difference between a
  // failure, which can be recovered from, and an error, which is a hard stop.
  struct Success {
    // The value this carries.
    std::tuple<Values...> value;
    std::vector<diag::Diagnostic> non_errors;
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

  using ResultValue = std::variant<Success, ParseError>;

  explicit ParseResultBase(ResultValue value) : value_(std::move(value)) {}

 private:
  ResultValue value_;
};

template <class... Values>
class ParseResult : public ParseResultBase<ParseResult, Values...> {
  using Base = ParseResultBase<ParseResult, Values...>;

 public:
  static ParseResult OfFailure(diag::Diagnostic error) {
    return ParseResult(ParseError::Failure({std::move(error)}));
  }

  static ParseResult OfError(diag::Diagnostic error) {
    return ParseResult(ParseError::Fatal({std::move(error)}));
  }

  ParseResult(Values&&... values) : Base(std::forward<Values>(values)...) {}
  ParseResult(ParseError error) : Base(std::move(error)) {}

 private:
  template <template <class...> class OtherType, class... OtherValues>
  friend class ParseResultBase;

  ParseResult(typename Base::ResultValue value) : Base(std::move(value)) {}
};

template <class Value>
class ParseResult<Value> : public ParseResultBase<ParseResult, Value> {
  using Base = ParseResultBase<ParseResult, Value>;

 public:
  static ParseResult OfFailure(diag::Diagnostic error) {
    return ParseResult(ParseError::Failure({std::move(error)}));
  }

  static ParseResult OfError(diag::Diagnostic error) {
    return ParseResult(ParseError::Fatal({std::move(error)}));
  }

  ParseResult(Value&& value) : Base(std::forward<Value>(value)) {}
  ParseResult(ParseError error) : Base(std::move(error)) {}

  Value const& value() const& { return std::get<0>(Base::values()); }
  Value&& value() && { return std::get<0>(std::move(*this).Base::values()); }

 private:
  template <template <class...> class OtherType, class... OtherValues>
  friend class ParseResultBase;

  ParseResult(typename Base::ResultValue value) : Base(std::move(value)) {}
};

template <class... Values>
ParseResult(Values&&...) -> ParseResult<std::decay_t<Values>...>;

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

}  // namespace parsers

#endif