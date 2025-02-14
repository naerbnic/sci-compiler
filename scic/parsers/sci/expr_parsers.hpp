#ifndef PARSERS_SCI_EXPR_PARSERS_HPP
#define PARSERS_SCI_EXPR_PARSERS_HPP

#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"

namespace parsers::sci {

ParseResult<Expr> ParseExpr(TreeExprSpan& exprs);

}

#endif