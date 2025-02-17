#ifndef UTIL_TYPES_OVERLOADS_HPP
#define UTIL_TYPES_OVERLOADS_HPP

#include <utility>

namespace util {
namespace internal {
template <typename... Ts>
struct OverloadImpl : Ts... {
  using Ts::operator()...;
};
}  // namespace internal

template <class... Fs>
auto Overload(Fs&&... fs) {
  return internal::OverloadImpl<Fs...>{std::forward<Fs>(fs)...};
}

}  // namespace util

#endif