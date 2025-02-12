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
      return ParseError(kind_, std::move(messages_));
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
  ParseError(Kind kind, std::vector<diag::Diagnostic> messages)
      : kind_(kind), messages_(std::move(messages)) {}

  Kind kind_ = EMPTY;
  std::vector<diag::Diagnostic> messages_;
};

template <class... Values>
class ParseResult {
 public:
  static ParseResult OfFailure(diag::Diagnostic error) {
    return ParseResult(ParseError::Failure({std::move(error)}));
  }
  static ParseResult OfError(diag::Diagnostic error) {
    return ParseResult(ParseError::Fatal({std::move(error)}));
  }

  ParseResult(Values&&... values)
      : ParseResult(Success{
            .value = std::tuple<Values...>(std::forward<Values>(values)...),
        }) {}

  ParseResult(ParseError error) : value_(error) {}

  ParseResult& AddWarning(diag::Diagnostic warning) {
    if (std::holds_alternative<Success>(value_)) {
      std::get<Success>(value_).non_errors_.push_back(std::move(warning));
    }
    return *this;
  }

  // Inverts the result, turning a success into a failure, and a failure into a
  // success. A fatal error remains a fatal error.
  ParseResult<> Invert() const {
    if (std::holds_alternative<Success>(value_)) {
      return ParseResult<>(ParseError::Failure({}));
    } else if (std::holds_alternative<ParseError>(value_)) {
      auto const& error = std::get<ParseError>(value_);
      if (error.kind() == ParseError::FAILURE) {
        return ParseResult<>();
      }
      return ParseResult<>(error);
    }
  }

  // Composes two parse results together, concatenating the values if both are
  // successful, or merging the errors if either is an error.
  template <class... MoreValues>
  ParseResult<Values..., MoreValues...> operator|(
      ParseResult<MoreValues...> const& other) const {
    if (std::holds_alternative<Success>(value_) &&
        std::holds_alternative<Success>(other.value_)) {
      auto& this_success = std::get<Success>(value_);
      auto& other_success = std::get<Success>(other.value_);
      return ParseResult<Values..., MoreValues...>(
          std::tuple_cat(this_success.value, other_success.value));
    }

    if (std::holds_alternative<ParseError>(value_) &&
        std::holds_alternative<ParseError>(other.value_)) {
      auto& this_error = std::get<ParseError>(value_);
      auto& other_error = std::get<ParseError>(other.value_);
      return ParseResult<Values..., MoreValues...>(ParseError::Failure(
          std::vector<diag::Diagnostic>(this_error | other_error)));
    }

    if (std::holds_alternative<ParseError>(value_)) {
      return ParseResult<Values..., MoreValues...>(
          std::get<ParseError>(value_));
    }

    return ParseResult<Values..., MoreValues...>(
        std::get<ParseError>(other.value_));
  }

 private:
  // This follows the general idea in Rust's nom, with a difference between a
  // failure, which can be recovered from, and an error, which is a hard stop.
  struct Success {
    // The value this carries.
    std::tuple<Values...> value;
    std::vector<diag::Diagnostic> non_errors;
  };

  using ResultValue = std::variant<Success, ParseError>;

  explicit ParseResult(ResultValue value) : value_(std::move(value)) {}

  ResultValue value_;
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