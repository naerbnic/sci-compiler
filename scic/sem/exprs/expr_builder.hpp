#ifndef SEM_EXPR_BUILDER_HPP
#define SEM_EXPR_BUILDER_HPP

#include <memory>

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/exprs/expr_context.hpp"

namespace sem {

std::unique_ptr<ExprContext> CreateExprContext(
    ExprEnvironment const* expr_env, codegen::CodeGenerator* codegen,
    codegen::FunctionBuilder* func_builder);

}  // namespace sem

#endif