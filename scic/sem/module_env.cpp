#include "scic/sem/module_env.hpp"

#include <map>
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
#include "scic/sem/input.hpp"
#include "scic/sem/object_table.hpp"
#include "scic/sem/proc_table.hpp"
#include "scic/sem/public_table.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {

namespace {

using codegen::CodeGenerator;

absl::StatusOr<ScriptNum> GetScriptId(Items items) {
  auto result = GetElemsOfTypes<ast::ScriptNumDef>(items);

  if (result.size() == 0) {
    return absl::InvalidArgumentError("No script number defined");
  } else if (result.size() > 1) {
    return absl::InvalidArgumentError("Multiple script numbers defined");
  }

  return ScriptNum::Create(result[0]->script_num().value());
}

absl::Status AddItemsToSelectorTable(SelectorTable::Builder* builder,
                                     absl::Span<ast::Item const> items) {
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

  return absl::OkStatus();
}

absl::StatusOr<codegen::LiteralValue> AstConstValueToLiteralValue(
    CodeGenerator* codegen, ast::ConstValue const& value) {
  return value.visit(
      [&](ast::NumConstValue const& num)
          -> absl::StatusOr<codegen::LiteralValue> {
        return num.value().value();
      },
      [&](ast::StringConstValue const& str)
          -> absl::StatusOr<codegen::LiteralValue> {
        if (!codegen) {
          return absl::InvalidArgumentError(
              "String constants cannot be used in this context");
        }
        return codegen->AddTextNode(str.value().value());
      });
}

absl::Status AddItemsToClassTable(ClassTableBuilder* builder,
                                  codegen::CodeGenerator* codegen,
                                  std::optional<ScriptNum> script_num,
                                  Items items) {
  for (auto const* class_decl : GetElemsOfTypes<ast::ClassDecl>(items)) {
    std::vector<ClassTableBuilder::Property> properties;
    for (auto const& prop : class_decl->properties()) {
      ASSIGN_OR_RETURN(auto value,
                       AstConstValueToLiteralValue(codegen, prop.value));
      properties.push_back(ClassTableBuilder::Property{
          .name = prop.name,
          .value = value,
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
    if (classdef->kind() != ast::ClassDef::CLASS) {
      continue;
    }

    if (!script_num) {
      return absl::InvalidArgumentError(
          "Can't process a classdef in a global context");
    }
    auto const& name = classdef->name();
    std::optional<NameToken> super_name;
    if (classdef->parent()) {
      super_name = *classdef->parent();
    }
    std::vector<ClassTableBuilder::Property> properties;
    for (auto const& prop : classdef->properties()) {
      auto const& prop_name = prop.name;
      ASSIGN_OR_RETURN(auto value,
                       AstConstValueToLiteralValue(codegen, prop.value));
      properties.push_back(ClassTableBuilder::Property{
          .name = prop_name,
          .value = value,
      });
    }

    std::vector<NameToken> methods;
    for (auto const& ast_method : classdef->methods()) {
      methods.push_back(ast_method.name());
    }

    RETURN_IF_ERROR(builder->AddClassDef(name, *script_num, super_name,
                                         std::move(properties),
                                         std::move(methods)));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ObjectTable>> BuildObjectTable(
    codegen::CodeGenerator* codegen, SelectorTable const* selector,
    ClassTable const* class_table, ScriptNum script_num,
    absl::Span<ast::Item const> items) {
  auto builder = ObjectTableBuilder::Create(codegen, selector, class_table);

  for (auto const* object : GetElemsOfTypes<ast::ClassDef>(items)) {
    if (object->kind() != ast::ClassDef::OBJECT) {
      continue;
    }

    if (!object->parent()) {
      return absl::InvalidArgumentError("Object has no parent class");
    }

    std::vector<ObjectTableBuilder::Property> properties;
    for (auto const& prop : object->properties()) {
      ASSIGN_OR_RETURN(auto value,
                       AstConstValueToLiteralValue(codegen, prop.value));
      properties.push_back(ObjectTableBuilder::Property{
          .name = prop.name,
          .value = value,
      });
    }
    std::vector<NameToken> methods;
    for (auto const& method_name : object->methods()) {
      methods.push_back(method_name.name());
    }
    RETURN_IF_ERROR(builder->AddObject(object->name(), script_num,
                                       *object->parent(), std::move(properties),
                                       std::move(methods)));
  }

  return builder->Build();
}

absl::StatusOr<std::unique_ptr<ProcTable>> BuildProcTable(
    codegen::CodeGenerator* codegen, absl::Span<ast::Item const> items) {
  auto builder = ProcTableBuilder::Create(codegen);

  for (auto const* proc : GetElemsOfTypes<ast::ProcDef>(items)) {
    RETURN_IF_ERROR(builder->AddProcedure(proc->name()));
  }

  return builder->Build();
}

absl::StatusOr<std::unique_ptr<PublicTable>> BuildPublicTable(
    ProcTable const* proc_table, ObjectTable const* object_table,
    absl::Span<ast::Item const> items) {
  auto builder = PublicTableBuilder::Create();

  auto elems = GetElemsOfTypes<ast::PublicDef>(items);

  if (elems.size() != 1) {
    return absl::InvalidArgumentError("Exactly one public table allowed");
  }

  auto const* public_def = elems[0];

  for (auto const& proc : public_def->entries()) {
    auto const* proc_entry = proc_table->LookupByName(proc.name.value());
    auto const* obj_entry = object_table->LookupByName(proc.name.value());
    if (!proc_entry && !obj_entry) {
      return absl::InvalidArgumentError("Entity not found.");
    }

    if (proc_entry && obj_entry) {
      return absl::InvalidArgumentError(
          "Ambiguous entity between objects and procedures.");
    }

    if (proc_entry) {
      RETURN_IF_ERROR(builder->AddProcedure(proc.index.value(), proc_entry));
    } else {
      RETURN_IF_ERROR(builder->AddObject(proc.index.value(), obj_entry));
    }
  }

  return builder->Build();
}

}  // namespace

absl::StatusOr<CompilationEnvironment> BuildCompilationEnvironment(
    codegen::SciTarget target, codegen::Optimization opt, Input const& input) {
  // Do initial processing, to get the script number from each module
  // and create the appropriate codegens.
  struct ModuleLocal {
    ScriptNum script_num;
    std::unique_ptr<codegen::CodeGenerator> codegen;
    Items items;
  };

  Items global_items = input.global_items;
  std::vector<ModuleLocal> modules;
  for (auto const& module : input.modules) {
    ASSIGN_OR_RETURN(auto script_num, GetScriptId(module.module_items));

    auto codegen = codegen::CodeGenerator::Create(target, opt);

    modules.emplace_back(ModuleLocal{
        .script_num = script_num,
        .codegen = std::move(codegen),
        .items = module.module_items,
    });
  }

  // Build selector table.
  auto selector_builder = SelectorTable::CreateBuilder();
  RETURN_IF_ERROR(
      AddItemsToSelectorTable(selector_builder.get(), global_items));
  for (auto const& module : modules) {
    RETURN_IF_ERROR(
        AddItemsToSelectorTable(selector_builder.get(), module.items));
  }

  ASSIGN_OR_RETURN(auto selector_table, selector_builder->Build());

  // Build class table.
  auto class_builder = ClassTableBuilder::Create(selector_table.get());
  RETURN_IF_ERROR(AddItemsToClassTable(class_builder.get(), nullptr,
                                       std::nullopt, global_items));
  for (auto const& module : modules) {
    RETURN_IF_ERROR(AddItemsToClassTable(class_builder.get(),
                                         module.codegen.get(),
                                         module.script_num, module.items));
  }

  ASSIGN_OR_RETURN(auto class_table, class_builder->Build());

  // We have all the information we need to build the global table.
  auto global_env = std::make_unique<GlobalEnvironment>(
      std::move(selector_table), std::move(class_table));

  // Now build the module environments.
  std::map<ScriptNum, std::unique_ptr<ModuleEnvironment>> module_envs;
  for (auto& module : modules) {
    ASSIGN_OR_RETURN(
        auto object_table,
        BuildObjectTable(module.codegen.get(), global_env->selector_table(),
                         global_env->class_table(), module.script_num,
                         module.items));

    ASSIGN_OR_RETURN(auto proc_table,
                     BuildProcTable(module.codegen.get(), module.items));

    ASSIGN_OR_RETURN(
        auto public_table,
        BuildPublicTable(proc_table.get(), object_table.get(), module.items));

    module_envs.emplace(module.script_num,
                        std::make_unique<ModuleEnvironment>(
                            global_env.get(), module.script_num,
                            std::move(module.codegen), std::move(object_table),
                            std::move(proc_table), std::move(public_table)));
  }

  return CompilationEnvironment(std::move(global_env), std::move(module_envs));
}

}  // namespace sem