#include "scic/sem/passes/build_code.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"
#include "util/types/overload.hpp"

namespace sem::passes {

using ::codegen::CodeGenerator;

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

absl::StatusOr<std::unique_ptr<SelectorTable>> BuildFromItems(
    absl::Span<ast::Item const> items) {
  auto builder = SelectorTable::CreateBuilder();
  {
    // First, gather declared selectors.
    auto classes = GetElemsOfTypes<ast::SelectorsDecl>(items);
    for (auto const* class_decl : classes) {
      auto const& selectors = class_decl->selectors();
      for (auto const& selector : selectors) {
        RETURN_IF_ERROR(builder->DeclareSelector(
            selector.name, SelectorNum::Create(selector.id.value())));
      }
    }
  }

  {
    auto classes = GetElemsOfTypes<ast::ClassDef>(items);
    std::vector<ast::TokenNode<util::RefStr>> result;
    for (auto const* class_def : classes) {
      for (auto const& prop : class_def->properties()) {
        RETURN_IF_ERROR(builder->AddNewSelector(prop.name));
      }
      for (auto const& method : class_def->methods()) {
        RETURN_IF_ERROR(builder->AddNewSelector(method.name()));
      }
    }
  }

  return builder->Build();
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

absl::StatusOr<std::vector<ast::TokenNode<util::RefStr>>> GatherNewSelectors(
    SelectorTable const* sel_table, absl::Span<ast::Item const> items) {
  auto classes = GetElemsOfTypes<ast::ClassDef>(items);
  std::vector<ast::TokenNode<util::RefStr>> result;
  for (auto const* class_def : classes) {
    for (auto const& prop : class_def->properties()) {
      result.push_back(prop.name);
    }
    for (auto const& method : class_def->methods()) {
      result.push_back(method.name());
    }
  }

  for (auto it = result.begin(); it != result.end();) {
    if (sel_table->LookupByName(it->value())) {
      it = result.erase(it);
    } else {
      ++it;
    }
  }

  return result;
}

static codegen::LiteralValue AstConstValueToLiteralValue(
    CodeGenerator* codegen, ast::ConstValue const& value) {
  return value.visit(
      [&](ast::NumConstValue const& num) -> codegen::LiteralValue {
        return num.value().value();
      },
      [&](ast::StringConstValue const& str) -> codegen::LiteralValue {
        return codegen->AddTextNode(str.value().value());
      });
}

absl::StatusOr<ClassDefTable> BuildClassDefTable(
    CodeGenerator* codegen, SelectorTable const* selectors,
    ClassDeclTable const* class_decls, absl::Span<ast::Item const> items) {
  auto classes = GetElemsOfTypes<ast::ClassDef>(items);

  ClassDefTable class_defs;
  for (auto const* classdef : classes) {
    auto const& name = classdef->name();
    std::vector<ClassDef::Property> properties;
    for (auto const& prop : classdef->properties()) {
      auto const& prop_name = prop.name;
      auto literal_value = AstConstValueToLiteralValue(codegen, prop.value);
      auto const* selector = selectors->LookupByName(prop_name.value());
      if (!selector) {
        return absl::InvalidArgumentError("Selector not found");
      }
      properties.push_back(ClassDef::Property{
          .name = prop_name,
          .selector = selector,
          .value = literal_value,
      });
    }

    std::vector<ClassDef::Method> methods;
    for (auto const& ast_method : classdef->methods()) {
      auto const& method_name = ast_method.name();
      auto const* selector = selectors->LookupByName(method_name.value());
      if (!selector) {
        return absl::InvalidArgumentError("Selector not found");
      }
      methods.push_back(ClassDef::Method{
          .name = method_name,
          .selector = selector,
      });
    }

    auto const* class_decl = class_decls->LookupByName(name.value());
    RETURN_IF_ERROR(class_defs.AddClassDef(
        name, class_decl, std::move(properties), std::move(methods)));
  }
  return absl::UnimplementedError("Not yet implemented.");
}

}  // namespace sem::passes