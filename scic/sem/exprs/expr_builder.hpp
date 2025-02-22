#ifndef SEM_EXPR_BUILDER_HPP
#define SEM_EXPR_BUILDER_HPP

#include <functional>
#include <map>
#include <memory>
#include <string_view>

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/exprs/expr_context.hpp"
#include "scic/sem/selector_table.hpp"

namespace sem {

std::unique_ptr<ExprContext> CreateExprContext(
    codegen::CodeGenerator* codegen, codegen::FunctionBuilder* func_builder,
    ClassTable const* class_table, SelectorTable const* selector_table,
    std::map<std::string_view, ExprContext::Sym, std::less<>> symbols);

}  // namespace sem

#endif