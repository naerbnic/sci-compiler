// Types for parser combinators.

#ifndef PARSERS_COMBINATORS_COMBINATORS_HPP
#define PARSERS_COMBINATORS_COMBINATORS_HPP

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/types/span.h"

namespace parsers {

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