#include "scic/sem/code_builder.hpp"

#include <string>

#include "absl/status/status.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/module_env.hpp"
#include "scic/sem/object_table.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_macros.hpp"

namespace sem {
namespace {

absl::Status BuildClass(ModuleEnvironment* module_env, Class const* class_def,
                        ast::ClassDef const& ast_node) {
  auto ptr_def = module_env->codegen()->CreatePtrRef();
  auto class_gen = module_env->codegen()->CreateClass(
      std::string(class_def->name()), &ptr_def);
  for (auto const& prop : class_def->prop_list().properties()) {
    auto const* selector = prop.selector();
    if (selector->name() == kMethDictSelName) {
      class_gen->AppendMethodTableProperty(std::string(class_def->name()),
                                           selector->selector_num().value());
    } else if (selector->name() == kPropDictSelName) {
      class_gen->AppendPropTableProperty(std::string(class_def->name()),
                                         selector->selector_num().value());
    } else {
      class_gen->AppendProperty(std::string(selector->name()),
                                selector->selector_num().value(), prop.value());
    }
  }

  for (auto const& method : ast_node.methods()) {
    auto const* selector =
        module_env->global_env()->selector_table()->LookupByName(
            method.name().value());
    auto meth_ptr_ref = module_env->codegen()->CreatePtrRef();
    class_gen->AppendMethod(std::string(method.name().value()),
                            selector->selector_num().value(), &meth_ptr_ref);

    // TODO: Build the method contents.
  }

  return absl::OkStatus();
}

absl::Status BuildObject(ModuleEnvironment* module_env, Object const* obj_def,
                         ast::ClassDef const& ast_node) {
  return absl::UnimplementedError("Not yet implemented");
}

}  // namespace

absl::Status BuildCode(ModuleEnvironment* module_env) {
  [[maybe_unused]] auto* codegen = module_env->codegen();
  // We handle every procedure, class, and object in the order it appears in the
  // AST. This matches the previous implementation, which does everything in
  // source order.
  for (auto const& elem : GetElemsOfTypes<ast::ProcDef, ast::ClassDef>(
           module_env->module_items())) {
    RETURN_IF_ERROR(elem.visit(
        [&](ast::ProcDef const* proc) { return absl::OkStatus(); },
        [&](ast::ClassDef const* class_def) {
          switch (class_def->kind()) {
            case ast::ClassDef::CLASS: {
              auto const* class_obj =
                  module_env->global_env()->class_table()->LookupByName(
                      class_def->name().value());
              return BuildClass(module_env, class_obj, *class_def);
            }

            case ast::ClassDef::OBJECT: {
              auto const* obj = module_env->object_table()->LookupByName(
                  class_def->name().value());
              return BuildObject(module_env, obj, *class_def);
            }

            default:
              return absl::InvalidArgumentError("Unknown class kind");
          }
          return absl::OkStatus();
        }));
  }

  return absl::OkStatus();
}

}  // namespace sem