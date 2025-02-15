#include "scic/parsers/sci/const_value_parsers.hpp"

#include <string>

#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"

namespace parsers::sci {

ParseResult<ConstValue> ParseConstValue(list_tree::TokenExpr const& expr) {
  if (auto num = expr.token().AsNumber()) {
    return ConstValue(
        NumConstValue(TokenNode<int>(num->value, expr.text_range())));
  } else if (auto str = expr.token().AsString()) {
    return ConstValue(StringConstValue(
        TokenNode<std::string>(str->decodedString, expr.text_range())));
  } else {
    return RangeFailureOf(expr.text_range(), "Expected number or string.");
  }
}

ParseResult<ConstValue> ParseOneConstValue(TreeExprSpan& exprs) {
  return ParseOneTreeExpr(ParseTokenExpr(ParseConstValue))(exprs);
}

}  // namespace parsers::sci