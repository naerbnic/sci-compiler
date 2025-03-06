#include "scic/sem/code_builder.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/exprs/expr_builder.hpp"
#include "scic/sem/exprs/expr_context.hpp"
#include "scic/sem/module_env.hpp"
#include "scic/sem/object_table.hpp"
#include "scic/sem/proc_table.hpp"
#include "scic/sem/property_list.hpp"
#include "scic/sem/selector_table.hpp"
#include "scic/status/status.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {
namespace {

using namespace ::util::ref_str_literals;

status::Status BuildGenericProcedure(
    ModuleEnvironment const* mod_env, PropertyList const* prop_list,
    std::optional<ExprEnvironment::SuperInfo> super_info,
    codegen::FuncName func_name, codegen::PtrRef* proc_ref,
    ast::ProcDef const& ast_node) {
  std::size_t curr_param_offset = 0;
  std::map<util::RefStr, ExprEnvironment::ParamSym, std::less<>> param_map;

  // "argc" is always the first parameter, giving a concrete number for the
  // number of parameters provided.
  param_map.emplace("argc"_rs, ExprEnvironment::ParamSym{
                                   .param_offset = curr_param_offset++,
                               });

  for (auto const& arg : ast_node.args()) {
    if (param_map.contains(arg.value())) {
      return status::InvalidArgumentError(
          absl::StrFormat("Duplicate parameter: %s", arg.value()));
    }
    param_map.emplace(arg.value(), ExprEnvironment::ParamSym{
                                       .param_offset = curr_param_offset++,
                                   });
  }

  std::size_t curr_temp_offset = 0;
  std::map<util::RefStr, ExprEnvironment::TempSym, std::less<>> temp_map;
  for (auto const& local : ast_node.locals()) {
    local.visit(
        [&](ast::SingleVarDef const& var) {
          temp_map.emplace(var.name().value(),
                           ExprEnvironment::TempSym{
                               .temp_offset = curr_temp_offset++,
                           });
        },
        [&](ast::ArrayVarDef const& index) {
          temp_map.emplace(index.name().value(),
                           ExprEnvironment::TempSym{
                               .temp_offset = curr_temp_offset,
                           });
          curr_temp_offset += index.size().value();
        });
  }
  auto expr_env =
      ExprEnvironment::Create(mod_env, prop_list, std::move(super_info),
                              std::move(param_map), std::move(temp_map));

  auto* codegen = mod_env->codegen();
  auto func_builder = codegen->CreateFunction(
      std::move(func_name), std::nullopt, curr_temp_offset, proc_ref);

  auto expr_context =
      CreateExprContext(expr_env.get(), mod_env->codegen(), func_builder.get());

  RETURN_IF_ERROR(expr_context->BuildExpr(ast_node.body()));

  // Add a trailing return.
  func_builder->AddReturnOp();

  return status::OkStatus();
}

status::Status BuildClass(ModuleEnvironment const* module_env,
                          Class const* class_def,
                          ast::ClassDef const& ast_node) {
  auto ptr_ref = module_env->codegen()->CreatePtrRef();
  auto class_gen = module_env->codegen()->CreateClass(
      std::string(class_def->name()), &ptr_ref);
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

  std::optional<ExprEnvironment::SuperInfo> super_info;

  if (class_def->super()) {
    super_info = ExprEnvironment::SuperInfo{
        .super_name = class_def->super()->token_name(),
        .species = class_def->super()->species(),
    };
  }

  for (auto const& method : ast_node.methods()) {
    auto const* selector =
        module_env->global_env()->selector_table()->LookupByName(
            method.name().value());
    auto meth_ptr_ref = module_env->codegen()->CreatePtrRef();
    class_gen->AppendMethod(std::string(method.name().value()),
                            selector->selector_num().value(), &meth_ptr_ref);

    codegen::MethodName name(std::string(class_def->name()),
                             std::string(method.name().value()));

    RETURN_IF_ERROR(BuildGenericProcedure(module_env, &class_def->prop_list(),
                                          super_info, std::move(name),
                                          &meth_ptr_ref, method));
  }

  return status::OkStatus();
}

status::Status BuildObject(ModuleEnvironment const* module_env,
                           Object const* obj_def,
                           ast::ClassDef const& ast_node) {
  auto obj_gen = module_env->codegen()->CreateObject(
      std::string(obj_def->name()), obj_def->ptr_ref());
  for (auto const& prop : obj_def->prop_list().properties()) {
    auto const* selector = prop.selector();
    if (selector->name() == kMethDictSelName) {
      obj_gen->AppendMethodTableProperty(std::string(obj_def->name()),
                                         selector->selector_num().value());
    } else if (selector->name() == kPropDictSelName) {
      obj_gen->AppendPropTableProperty(std::string(obj_def->name()),
                                       selector->selector_num().value());
    } else {
      obj_gen->AppendProperty(std::string(selector->name()),
                              selector->selector_num().value(), prop.value());
    }
  }

  auto super_info = ExprEnvironment::SuperInfo{
      .super_name = obj_def->parent()->token_name(),
      .species = obj_def->parent()->species(),
  };

  for (auto const& method : ast_node.methods()) {
    auto const* selector =
        module_env->global_env()->selector_table()->LookupByName(
            method.name().value());
    auto meth_ptr_ref = module_env->codegen()->CreatePtrRef();
    obj_gen->AppendMethod(std::string(method.name().value()),
                          selector->selector_num().value(), &meth_ptr_ref);

    codegen::MethodName name(std::string(obj_def->name()),
                             std::string(method.name().value()));

    RETURN_IF_ERROR(BuildGenericProcedure(module_env, &obj_def->prop_list(),
                                          super_info, std::move(name),
                                          &meth_ptr_ref, method));
  }

  return status::OkStatus();
}

status::Status BuildProcedure(ModuleEnvironment const* module_env,
                              Procedure const* proc_obj,
                              ast::ProcDef const& ast_node) {
  codegen::ProcedureName name(std::string(proc_obj->name()));

  return BuildGenericProcedure(module_env, nullptr, std::nullopt,
                               std::move(name), proc_obj->ptr_ref(), ast_node);
}

}  // namespace

status::Status BuildCode(ModuleEnvironment const* module_env) {
  auto* codegen = module_env->codegen();
  // Add variables to the current module based on the local table.
  for (auto const& local_var : module_env->local_table()->vars()) {
    auto initial_value = local_var.initial_value();
    auto base_index = local_var.index().value();
    for (int i = 0; i < initial_value.size(); ++i) {
      codegen->SetVar(base_index + i, initial_value[i]);
    }
  }
  // We handle every procedure, class, and object in the order it appears in the
  // AST. This matches the previous implementation, which does everything in
  // source order.
  for (auto const& elem : GetElemsOfTypes<ast::ProcDef, ast::ClassDef>(
           module_env->module_items())) {
    RETURN_IF_ERROR(elem.visit(
        [&](ast::ProcDef const* proc) {
          auto const* proc_obj =
              module_env->proc_table()->LookupByName(proc->name().value());
          return BuildProcedure(module_env, proc_obj, *proc);
        },
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
              return status::InvalidArgumentError("Unknown class kind");
          }
          return status::OkStatus();
        }));
  }

  // Add public declarations for the current module.
  for (auto const& entry : module_env->public_table()->entries()) {
    entry.value().visit(
        [&](Procedure const* proc) {
          codegen->AddPublic(std::string(proc->name()), entry.index(),
                             proc->ptr_ref());
        },
        [&](Object const* obj) {
          codegen->AddPublic(std::string(obj->name()), entry.index(),
                             obj->ptr_ref());
        },
        [&](Class const* cls) {
          codegen->AddPublic(std::string(cls->name()), entry.index(),
                             cls->class_ref());
        });
  }

  return status::OkStatus();
}

}  // namespace sem