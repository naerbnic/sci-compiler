#ifndef PARSERS_SCI_CONST_VALUE_PARSERS_HPP
#define PARSERS_SCI_CONST_VALUE_PARSERS_HPP

#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"

namespace parsers::sci {

ParseResult<ConstValue> ParseConstValue(list_tree::TokenExpr const& expr);
ParseResult<ConstValue> ParseOneConstValue(TreeExprSpan& exprs);

}  // namespace parsers::sci

#endif