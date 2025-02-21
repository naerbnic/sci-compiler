#include "scic/sem/passes/build_code.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "util/status/status_macros.hpp"
#include "util/types/choice.hpp"
#include "util/types/overload.hpp"
#include "util/types/tmpl.hpp"

namespace sem::passes {

using ::codegen::CodeGenerator;

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

template <class... Fs>
absl::Status StatusVisitItems(Items items, Fs&&... fs) {
  for (auto const& item : items) {
    auto status = item.visit(std::forward<Fs>(fs)...);
    if (!status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

template <class T, class... Fs>
auto VectorVisitItems(Items items, Fs&&... fs) {
  std::vector<T> result;
  for (auto const& item : items) {
    auto overload = util::Overload(std::forward<Fs>(fs)...);
    item.visit([&](auto&& value) {
      auto inner_result = overload(value);
      using ResultType = std::decay_t<decltype(inner_result)>;
      if constexpr (std::ranges::range<ResultType>) {
        std::ranges::move(std::move(inner_result), std::back_inserter(result));
      } else {
        static_assert(
            std::is_same_v<ResultType, T>,
            "Result type must be a range of items that are convertible to T");
      }
    });
  }
  return result;
}

absl::StatusOr<std::size_t> GetScriptId(Items items) {
  auto result = GetElemsOfTypes<ast::ScriptNumDef>(items);

  if (result.size() == 0) {
    return absl::InvalidArgumentError("No script number defined");
  } else if (result.size() > 1) {
    return absl::InvalidArgumentError("Multiple script numbers defined");
  }

  return result[0]->script_num().value();
}

absl::StatusOr<SelectorTable> BuildSelectorTable(
    absl::Span<ast::Item const> items) {
  SelectorTable sel_table;
  auto classes = GetElemsOfTypes<ast::SelectorsDecl>(items);
  for (auto const* class_decl : classes) {
    auto const& selectors = class_decl->selectors();
    for (auto const& selector : selectors) {
      RETURN_IF_ERROR(
          sel_table.InstallSelector(selector.name, selector.id.value()));
    }
  }

  return sel_table;
}

// Reliably sets the value to a machine word, signed or unsigned.
absl::StatusOr<std::uint16_t> ConvertToMachineWord(int value) {
  auto narrowed = static_cast<std::int16_t>(value);
  if (narrowed != value) {
    // We've changed the value, so it must be out of range.
    return absl::InvalidArgumentError("Value is too large for a machine word");
  }
  return std::bit_cast<std::uint16_t>(narrowed);
}

absl::StatusOr<ClassDeclTable> BuildClassDeclGraph(
    SelectorTable const* sel_table, absl::Span<ast::Item const> items) {
  auto classes = GetElemsOfTypes<ast::ClassDecl>(items);

  ClassDeclTable class_decls;
  // First, create all of the class declarations. We'll patch in the other
  // values later.
  for (auto const* class_decl : classes) {
    std::size_t class_num = class_decl->class_num().value();

    std::vector<ClassDecl::Property> properties;
    for (auto const& prop : class_decl->properties()) {
      auto entry = sel_table->LookupByName(prop.name.value());
      if (!entry) {
        return absl::InvalidArgumentError("Property not found");
      }

      if (!prop.value.has<ast::NumConstValue>()) {
        return absl::InvalidArgumentError(
            "Property value must be a number in a class declaration.");
      }

      ASSIGN_OR_RETURN(
          auto value, ConvertToMachineWord(
                          prop.value.as<ast::NumConstValue>().value().value()));

      properties.push_back(ClassDecl::Property{
          .name = prop.name,
          .selector = entry,
          .value = value,
      });
    }

    std::vector<SelectorTable::Entry const*> methods;
    for (auto const& method_name : class_decl->method_names().names) {
      auto entry = sel_table->LookupByName(method_name.value());
      if (!entry) {
        return absl::InvalidArgumentError("Method not found");
      }
      methods.push_back(entry);
    }

    RETURN_IF_ERROR(class_decls.AddClass(
        class_decl->name(), class_num, class_decl->script_num().value(),
        std::move(properties), std::move(methods)));
  }

  // Patch in the super classes.
  for (auto const* class_decl : classes) {
    std::size_t class_num = class_decl->class_num().value();
    std::optional<std::size_t> super_num;

    if (class_decl->parent_num()) {
      super_num = class_decl->parent_num()->value();
    }

    RETURN_IF_ERROR(class_decls.SetClassSuper(class_num, super_num));
  }

  return class_decls;
}

}  // namespace sem::passes