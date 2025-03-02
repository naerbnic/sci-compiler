#include "scic/sem/module_env.hpp"

#include <cstddef>
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
#include "scic/sem/extern_table.hpp"
#include "scic/sem/input.hpp"
#include "scic/sem/object_table.hpp"
#include "scic/sem/proc_table.hpp"
#include "scic/sem/public_table.hpp"
#include "scic/sem/selector_table.hpp"
#include "scic/sem/var_table.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

namespace {

using codegen::CodeGenerator;

absl::StatusOr<ScriptNum> GetScriptId(Items items) {
  auto result = GetElemsOfType<ast::ScriptNumDef>(items);

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
    auto classes = GetElemsOfType<ast::SelectorsDecl>(items);
    for (auto const* class_decl : classes) {
      auto const& selectors = class_decl->selectors();
      for (auto const& selector : selectors) {
        RETURN_IF_ERROR(builder->DeclareSelector(
            selector.name, SelectorNum::Create(selector.id.value())));
      }
    }
  }

  {
    auto classes = GetElemsOfType<ast::ClassDef>(items);
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
  for (auto const* class_decl : GetElemsOfType<ast::ClassDecl>(items)) {
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

  for (auto const* classdef : GetElemsOfType<ast::ClassDef>(items)) {
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

// Do initial processing, to get the script number from each module
// and create the appropriate codegens.
struct ModuleLocal {
  ScriptNum script_num;
  std::unique_ptr<codegen::CodeGenerator> codegen;
  Items items;
};

absl::StatusOr<std::unique_ptr<SelectorTable>> BuildSelectorTable(
    Items global_items, util::SeqView<ModuleLocal const> modules) {
  auto selector_builder = SelectorTable::CreateBuilder();

  RETURN_IF_ERROR(
      AddItemsToSelectorTable(selector_builder.get(), global_items));
  for (auto const& module : modules) {
    RETURN_IF_ERROR(
        AddItemsToSelectorTable(selector_builder.get(), module.items));
  }

  return selector_builder->Build();
}

absl::StatusOr<std::unique_ptr<ClassTable>> BuildClassTable(
    SelectorTable const* selector_table, Items global_items,
    util::SeqView<ModuleLocal const> modules) {
  auto class_builder = ClassTableBuilder::Create(selector_table);
  RETURN_IF_ERROR(AddItemsToClassTable(class_builder.get(), nullptr,
                                       std::nullopt, global_items));
  for (auto const& module : modules) {
    RETURN_IF_ERROR(AddItemsToClassTable(class_builder.get(),
                                         module.codegen.get(),
                                         module.script_num, module.items));
  }

  return class_builder->Build();
}

absl::StatusOr<std::unique_ptr<ExternTable>> BuildExternTable(
    absl::Span<ast::Item const> items) {
  auto builder = ExternTableBuilder::Create();

  for (auto const* extern_def : GetElemsOfType<ast::ExternDef>(items)) {
    for (auto const& entry : extern_def->entries()) {
      auto module_num = entry.module_num.value();
      if (module_num < -1) {
        return absl::InvalidArgumentError(
            "Module number must be -1 or greater");
      }
      std::optional<ScriptNum> script_num;
      if (module_num >= 0) {
        script_num = ScriptNum::Create(module_num);
      }

      auto public_index = entry.index.value();
      if (public_index < 0) {
        return absl::InvalidArgumentError("Public index must be 0 or greater");
      }

      RETURN_IF_ERROR(builder->AddExtern(entry.name, script_num,
                                         PublicIndex::Create(public_index)));
    }
  }

  return builder->Build();
}

absl::StatusOr<std::unique_ptr<VarDeclTable>> BuildGlobalTable(
    absl::Span<ast::Item const> items) {
  auto builder = VarDeclTableBuilder::Create();

  for (auto const* var_decl : GetElemsOfType<ast::GlobalDeclDef>(items)) {
    for (auto const& entry : var_decl->entries()) {
      auto name = entry.name.visit(
          [&](ast::SingleVarDef const& single_var) {
            return single_var.name();
          },
          [&](ast::ArrayVarDef array_var) { return array_var.name(); });
      RETURN_IF_ERROR(builder->DeclareVar(name, entry.index.value(), 1));
    }
  }

  return builder->Build();
}

absl::StatusOr<std::unique_ptr<ObjectTable>> BuildObjectTable(
    codegen::CodeGenerator* codegen, SelectorTable const* selector,
    ClassTable const* class_table, ScriptNum script_num,
    absl::Span<ast::Item const> items) {
  auto builder = ObjectTableBuilder::Create(codegen, selector, class_table);

  for (auto const* object : GetElemsOfType<ast::ClassDef>(items)) {
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

  for (auto const* proc : GetElemsOfType<ast::ProcDef>(items)) {
    RETURN_IF_ERROR(builder->AddProcedure(proc->name()));
  }

  return builder->Build();
}

absl::StatusOr<std::unique_ptr<PublicTable>> BuildPublicTable(
    ProcTable const* proc_table, ObjectTable const* object_table,
    absl::Span<ast::Item const> items) {
  auto builder = PublicTableBuilder::Create();

  auto elems = GetElemsOfType<ast::PublicDef>(items);

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

absl::StatusOr<std::vector<codegen::LiteralValue>>
AstConstValuesToLiteralValues(codegen::CodeGenerator* codegen,
                              util::Seq<ast::ConstValue const&> values,
                              std::size_t expected_length) {
  std::vector<codegen::LiteralValue> result;
  if (values.size() == 1) {
    ASSIGN_OR_RETURN(auto literal_value,
                     AstConstValueToLiteralValue(codegen, values[0]));
    for (int i = 0; i < expected_length; ++i) {
      result.push_back(literal_value);
    }
    return result;
  }

  if (values.size() != expected_length) {
    return absl::InvalidArgumentError(
        "Array length does not match number of initial values");
  }

  for (auto const& value : values) {
    ASSIGN_OR_RETURN(auto literal_value,
                     AstConstValueToLiteralValue(codegen, value));
    result.push_back(literal_value);
  }

  return result;
}

absl::StatusOr<std::unique_ptr<VarTable>> BuildLocalTable(
    CodeGenerator* codegen, absl::Span<ast::Item const> items) {
  auto builder = VarTableBuilder::Create();

  for (auto const* var_decl : GetElemsOfType<ast::ModuleVarsDef>(items)) {
    for (auto const& entry : var_decl->entries()) {
      using VisitResult = absl::StatusOr<std::pair<NameToken, std::size_t>>;
      ASSIGN_OR_RETURN(
          auto result,
          entry.name.visit(
              [&](ast::SingleVarDef const& single_var) -> VisitResult {
                return std::make_pair(single_var.name(), 1);
              },
              [&](ast::ArrayVarDef array_var) -> VisitResult {
                return std::make_pair(array_var.name(),
                                      array_var.size().value());
              }));
      auto [name, length] = std::move(result);
      if (!entry.initial_value) {
        return absl::FailedPreconditionError("No initial value");
      }
      ASSIGN_OR_RETURN(
          auto initial_value,
          AstConstValuesToLiteralValues(
              codegen,
              entry.initial_value->visit(
                  [&](ast::ArrayInitialValue const& array)
                      -> util::Seq<ast::ConstValue const&> {
                    return array.value();
                  },
                  [&](ast::ConstValue const& value)
                      -> util::Seq<ast::ConstValue const&> {
                    return util::Seq<ast::ConstValue const&>::Singleton(value);
                  }),
              length));
      RETURN_IF_ERROR(builder->DefineVar(name, entry.index.value(),
                                         std::move(initial_value)));
    }
  }

  return builder->Build();
}

absl::StatusOr<std::unique_ptr<GlobalEnvironment>> BuildGlobalEnvironment(
    Items global_items, util::SeqView<ModuleLocal const> modules) {
  ASSIGN_OR_RETURN(auto selector_table,
                   BuildSelectorTable(global_items, modules));
  ASSIGN_OR_RETURN(auto class_table, BuildClassTable(selector_table.get(),
                                                     global_items, modules));

  ASSIGN_OR_RETURN(auto extern_table, BuildExternTable(global_items));

  ASSIGN_OR_RETURN(auto global_table, BuildGlobalTable(global_items));

  return std::make_unique<GlobalEnvironment>(
      std::move(selector_table), std::move(class_table),
      std::move(extern_table), std::move(global_table), global_items);
}

absl::StatusOr<std::unique_ptr<ModuleEnvironment>> BuildModuleEnvironment(
    GlobalEnvironment const* global_env, ScriptNum script_num,
    std::unique_ptr<CodeGenerator> codegen, Items module_items) {
  ASSIGN_OR_RETURN(
      auto object_table,
      BuildObjectTable(codegen.get(), global_env->selector_table(),
                       global_env->class_table(), script_num, module_items));

  ASSIGN_OR_RETURN(auto proc_table,
                   BuildProcTable(codegen.get(), module_items));

  ASSIGN_OR_RETURN(
      auto public_table,
      BuildPublicTable(proc_table.get(), object_table.get(), module_items));

  ASSIGN_OR_RETURN(auto locals_table,
                   BuildLocalTable(codegen.get(), module_items));

  return std::make_unique<ModuleEnvironment>(
      global_env, script_num, std::move(codegen), std::move(object_table),
      std::move(proc_table), std::move(public_table), std::move(locals_table),
      module_items);
}

}  // namespace

absl::StatusOr<CompilationEnvironment> BuildCompilationEnvironment(
    codegen::CodeGenerator::Options codegen_options, Input const& input) {
  Items global_items = input.global_items;
  std::vector<ModuleLocal> modules;
  for (auto const& module : input.modules) {
    ASSIGN_OR_RETURN(auto script_num, GetScriptId(module.module_items));

    auto codegen = codegen::CodeGenerator::Create(std::move(codegen_options));

    modules.emplace_back(ModuleLocal{
        .script_num = script_num,
        .codegen = std::move(codegen),
        .items = module.module_items,
    });
  }

  ASSIGN_OR_RETURN(auto global_env,
                   BuildGlobalEnvironment(global_items, modules));

  // Now build the module environments.
  std::map<ScriptNum, std::unique_ptr<ModuleEnvironment>> module_envs;
  for (auto& module : modules) {
    ASSIGN_OR_RETURN(
        auto module_env,
        BuildModuleEnvironment(global_env.get(), module.script_num,
                               std::move(module.codegen), module.items));
    module_envs.emplace(module_env->script_num(), std::move(module_env));
  }

  return CompilationEnvironment(std::move(global_env), std::move(module_envs));
}

}  // namespace sem