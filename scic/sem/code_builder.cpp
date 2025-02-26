#include "scic/sem/code_builder.hpp"

#include <string>

#include "absl/status/status.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/module_env.hpp"
#include "util/status/status_macros.hpp"

namespace sem {
namespace {
absl::Status BuildClass(ModuleEnvironment* module_env, Class const* class_def,
                        ast::ClassDef const& ast_node) {
  auto ptr_def = module_env->codegen()->CreatePtrRef();
  auto class_gen = module_env->codegen()->CreateClass(
      std::string(class_def->name()), &ptr_def);
  for (auto const& method : ast_node.methods()) {
  }
  return absl::OkStatus();
}

absl::Status BuildObject(ModuleEnvironment* module_env, Class const* class_def,
                         ast::ClassDef const& ast_node) {
  auto const* class_obj =
      module_env->global_env()->class_table()->LookupByName(class_def->name());
  for (auto const& method : class_def->methods()) {
    // auto const& method_obj = class_obj->GetMethod(method.name());
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status BuildCode(ModuleEnvironment* module_env) {
  [[maybe_unused]] auto* codegen = module_env->codegen();
  // We handle every procedure, class, and object as it appears in the AST.
  for (auto const& elem : GetElemsOfTypes<ast::ProcDef, ast::ClassDef>(
           module_env->module_items())) {
    RETURN_IF_ERROR(
        elem.visit([&](ast::ProcDef const* proc) { return absl::OkStatus(); },
                   [&](ast::ClassDef const* class_def) {
                     auto const* class_obj =
                         module_env->global_env()->class_table()->LookupByName(
                             class_def->name().value());
                     switch (class_def->kind()) {
                       case ast::ClassDef::CLASS:
                         return BuildClass(module_env, class_obj, *class_def);

                       case ast::ClassDef::OBJECT:
                         return BuildObject(module_env, class_obj, *class_def);
                     }
                     if (class_def->kind() != ast::ClassDef::CLASS) {
                       return absl::OkStatus();
                     }
                     return absl::OkStatus();
                   }));
  }

  return absl::OkStatus();
}

}  // namespace sem