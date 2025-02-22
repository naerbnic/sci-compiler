#include "scic/sem/passes/build_code.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"

namespace sem::passes {

using ::codegen::CodeGenerator;

absl::StatusOr<ScriptNum> GetScriptId(Items items) {
  auto result = GetElemsOfTypes<ast::ScriptNumDef>(items);

  if (result.size() == 0) {
    return absl::InvalidArgumentError("No script number defined");
  } else if (result.size() > 1) {
    return absl::InvalidArgumentError("Multiple script numbers defined");
  }

  return ScriptNum::Create(result[0]->script_num().value());
}

absl::StatusOr<std::unique_ptr<SelectorTable>> BuildSelectorTableFromItems(
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

absl::StatusOr<std::unique_ptr<ClassTable>> BuildClassTableFromItems(
    CodeGenerator* codegen, SelectorTable const* selectors,
    ScriptNum script_num, absl::Span<ast::Item const> items) {
  auto builder = ClassTableBuilder::Create(selectors);

  for (auto const* class_decl : GetElemsOfTypes<ast::ClassDecl>(items)) {
    std::vector<ClassTableBuilder::Property> properties;
    for (auto const& prop : class_decl->properties()) {
      properties.push_back(ClassTableBuilder::Property{
          .name = prop.name,
          .value = AstConstValueToLiteralValue(codegen, prop.value),
      });
    }
    std::vector<NameToken> methods;
    for (auto const& method_name : class_decl->method_names().names) {
      methods.push_back(method_name);
    }
    std::optional<ClassSpecies> super_species;
    if (class_decl->parent_num()) {
      super_species = ClassSpecies::Create(class_decl->parent_num()->value());
    }
    RETURN_IF_ERROR(builder->AddClassDecl(
        class_decl->name(), ScriptNum::Create(class_decl->script_num().value()),
        ClassSpecies::Create(class_decl->class_num().value()), super_species,
        std::move(properties), std::move(methods)));
  }

  for (auto const* classdef : GetElemsOfTypes<ast::ClassDef>(items)) {
    auto const& name = classdef->name();
    std::optional<NameToken> super_name;
    if (classdef->parent()) {
      super_name = *classdef->parent();
    }
    std::vector<ClassTableBuilder::Property> properties;
    for (auto const& prop : classdef->properties()) {
      auto const& prop_name = prop.name;
      properties.push_back(ClassTableBuilder::Property{
          .name = prop_name,
          .value = AstConstValueToLiteralValue(codegen, prop.value),
      });
    }

    std::vector<NameToken> methods;
    for (auto const& ast_method : classdef->methods()) {
      methods.push_back(ast_method.name());
    }

    RETURN_IF_ERROR(builder->AddClassDef(name, script_num, super_name,
                                         std::move(properties),
                                         std::move(methods)));
  }

  return builder->Build();
}

}  // namespace sem::passes