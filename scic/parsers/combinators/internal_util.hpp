#ifndef PARSERS_COMBINATORS_INTERNAL_UTIL_HPP
#define PARSERS_COMBINATORS_INTERNAL_UTIL_HPP

#include <vector>

namespace parsers {
namespace internal {

template <class T>
std::vector<T> ConcatVectors(std::vector<T> a, std::vector<T> b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

}  // namespace internal
}  // namespace parsers

#endif