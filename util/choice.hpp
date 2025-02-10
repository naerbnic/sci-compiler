#ifndef UTIL_OVERLOADED_HPP
#define UTIL_OVERLOADED_HPP

#include <utility>
#include <variant>

namespace util {

// This uses an approach in the C++ reference to create a class from lambdas
// that can be used in a std::visit call, or equivalent.
template <class... Ts>
struct Visitor : Ts... {
  using Ts::operator()...;
};

template <class BaseT, class... Types>
class ChoiceBase {
 public:
  template <class T, class... Args>
  static BaseT Make(Args&&... args) {
    return BaseT(std::in_place_type<T>, std::forward<Args>(args)...);
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
  decltype(auto) visit(Fs&&... fs) {
    return std::visit(Visitor<Fs...>{std::forward<Fs>(fs)...}, value_);
  }

 protected:
  template <class T, class... Args>
  ChoiceBase(std::in_place_type_t<T>, Args&&... args)
      : value_(std::in_place_type<T>, std::forward<Args>(args)...) {}
  std::variant<Types...> value_;
};

}  // namespace util
#endif