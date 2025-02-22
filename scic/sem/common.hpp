#ifndef SEM_COMMON_HPP
#define SEM_COMMON_HPP

#include <vector>

#include "absl/types/span.h"
#include "scic/parsers/sci/ast.hpp"
#include "util/types/tmpl.hpp"

namespace sem {

namespace ast = parsers::sci;
using Items = absl::Span<ast::Item const>;

template <class... Fs>
void VisitItems(Items items, Fs&&... fs) {
  for (auto const& item : items) {
    item.visit(std::forward<Fs>(fs)...);
  }
}

template <class... Ts>
auto GetElemsOfTypes(auto const& range) {
  if constexpr (sizeof...(Ts) == 1) {
    // There's no need to use a choice type if there's only one type.
    using Result = util::TypePack<Ts...>::template TypeAt<0> const*;
    std::vector<Result> result;
    for (auto const& item : range) {
      item.visit([&](Ts const& elem) { result.push_back(&elem); }...,
                 [&](auto&&) {
                   // Not desired, so remove.
                 });
    }
    return result;

  } else if (sizeof...(Ts) > 1) {
    using Result = util::Choice<Ts const*...>;
    std::vector<Result> result;
    for (auto const& item : range) {
      item.visit([&](Ts const& elem) { result.push_back(Result(&elem)); }...,
                 [&](auto&&) {
                   // Not desired, so remove.
                 });
    }
    return result;
  } else {
    static_assert(sizeof...(Ts) > 0, "At least one type must be specified");
  }
}

}  // namespace sem

#endif  // SEM_COMMON_HPP