#ifndef UTIL_OVERLOADED_HPP
#define UTIL_OVERLOADED_HPP

#include <ostream>
#include <utility>
#include <variant>

#include "absl/strings/has_absl_stringify.h"

namespace util {

// This uses an approach in the C++ reference to create a class from lambdas
// that can be used in a std::visit call, or equivalent.
template <class... Ts>
struct Visitor : Ts... {
  using Ts::operator()...;
};

// A base type used to create choice types.
template <class Derived, class... Types>
class ChoiceBase {
 public:
  template <class T, class... Args>
  static Derived Make(Args&&... args) {
    return Derived(std::in_place_type<T>, std::forward<Args>(args)...);
  }

  ChoiceBase() = default;
  ChoiceBase(ChoiceBase const&) = default;
  ChoiceBase(ChoiceBase&&) = default;
  ChoiceBase& operator=(ChoiceBase const&) = default;
  ChoiceBase& operator=(ChoiceBase&&) = default;

  template <class T>
  ChoiceBase(T&& value) : value_(std::forward<T>(value)) {}

  template <class T>
  T const& as() const {
    return std::get<T>(value_);
  }

  template <class T>
  T& as() {
    return std::get<T>(value_);
  }

  template <class T>
  T const* try_get() const {
    auto* ptr = std::get_if<T>(&value_);
    return ptr ? ptr : nullptr;
  }

  template <class T>
  T* try_get() {
    auto* ptr = std::get_if<T>(&value_);
    return ptr ? ptr : nullptr;
  }

  template <class T>
  bool has() const {
    return std::holds_alternative<T>(value_);
  }

  template <class... Fs>
  decltype(auto) visit(Fs&&... fs) const& {
    return std::visit(Visitor<Fs...>{std::forward<Fs>(fs)...}, value_);
  }

  template <class... Fs>
  decltype(auto) visit(Fs&&... fs) & {
    return std::visit(Visitor<Fs...>{std::forward<Fs>(fs)...}, value_);
  }

  template <class... Fs>
  decltype(auto) visit(Fs&&... fs) && {
    return std::visit(Visitor<Fs...>{std::forward<Fs>(fs)...},
                      std::move(value_));
  }

 protected:
  template <class T, class... Args>
  ChoiceBase(std::in_place_type_t<T>, Args&&... args)
      : value_(std::in_place_type<T>, std::forward<Args>(args)...) {}

 private:
  std::variant<Types...> value_;

  friend std::ostream& operator<<(std::ostream& os, Derived const& choice)
    requires(requires(Types const& t, std::ostream& os) {
      { os << t } -> std::same_as<std::ostream&>;
    } && ...)
  {
    return choice.visit(
        [&os](auto const& value) -> std::ostream& { return (os << value); });
  }

  template <class Sink>
  friend void AbslStringify(Sink& sink, Derived const& choice)
    requires(absl::HasAbslStringify<Types>() && ...)
  {
    choice.visit([&sink](auto const& value) { AbslStringify(sink, value); });
  }
};

// A generic class, intended to be a more ergonomic replacement for
// std::variant.
template <class... Args>
class Choice : public ChoiceBase<Choice<Args...>, Args...> {
 public:
  using ChoiceBase<Choice<Args...>, Args...>::ChoiceBase;
};

}  // namespace util
#endif