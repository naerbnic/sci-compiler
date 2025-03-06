#ifndef SEM_COMMON_HPP
#define SEM_COMMON_HPP

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/status/status.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"
#include "util/types/strong_types.hpp"

namespace sem {

// Import the parsers::sci namespace as namespace ast
namespace ast {
using namespace parsers::sci;
}

using Items = absl::Span<ast::Item const>;

using NameToken = ast::TokenNode<util::RefStr>;

template <class... Fs>
void VisitItems(Items items, Fs&&... fs) {
  for (auto const& item : items) {
    item.visit(std::forward<Fs>(fs)...);
  }
}

template <class... Ts>
auto GetElemsOfTypes(auto const& range) {
  using Result = util::Choice<Ts const*...>;
  std::vector<Result> result;
  for (auto const& item : range) {
    item.visit([&](Ts const& elem) { result.push_back(Result(&elem)); }...,
               [&](auto&&) {
                 // Not desired, so remove.
               });
  }
  return result;
}

template <class T>
auto GetElemsOfType(auto const& range) {
  // There's no need to use a choice type if there's only one type.
  std::vector<T const*> result;
  for (auto const& item : range) {
    item.visit([&](T const& elem) { result.push_back(&elem); },
               [&](auto&&) {
                 // Not desired, so remove.
               });
  }
  return result;
}

// Strong types for common concepts in the sem namespace.

// A tag that contains a std::size_t value.
struct SizeTag {
  using Value = std::size_t;
  static constexpr bool is_const = true;
};

// A tag that contains a RefStr value.
struct RefStrTag {
  using Value = util::RefStr;
  static constexpr bool is_const = true;

  using View = std::string_view;
  using ViewStorage = std::string_view;

  static ViewStorage ValueToStorage(Value const& value) { return value; }
  static View StorageToView(ViewStorage const& storage) { return storage; }
};

struct ScriptNumTag : SizeTag {};
using ScriptNum = util::StrongValue<ScriptNumTag>;

struct ClassSpeciesTag : SizeTag {};
using ClassSpecies = util::StrongValue<ClassSpeciesTag>;

struct SelectorNumTag : SizeTag {};
using SelectorNum = util::StrongValue<SelectorNumTag>;

struct PublicIndexTag : SizeTag {};
using PublicIndex = util::StrongValue<PublicIndexTag>;

struct PropIndexTag : SizeTag {};
using PropIndex = util::StrongValue<PropIndexTag>;

struct GlobalIndexTag : SizeTag {};
using GlobalIndex = util::StrongValue<GlobalIndexTag>;

struct ModuleVarIndexTag : SizeTag {};
using ModuleVarIndex = util::StrongValue<ModuleVarIndexTag>;

// Reliably sets the value to a machine word, signed or unsigned.
inline status::StatusOr<std::uint16_t> ConvertToMachineWord(int value) {
  auto narrowed_i16 = static_cast<std::int16_t>(value);
  if (narrowed_i16 == value) {
    return std::bit_cast<std::uint16_t>(narrowed_i16);
  }

  auto narrowed_u16 = static_cast<std::uint16_t>(value);
  if (narrowed_u16 == value) {
    return narrowed_u16;
  }

  // We've changed the value, so it must be out of range.
  return status::InvalidArgumentError(
      absl::StrFormat("Value is too large for a machine word: %x", value));
}

}  // namespace sem

#endif  // SEM_COMMON_HPP