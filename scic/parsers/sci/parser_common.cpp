#include "scic/parsers/sci/parser_common.hpp"

#include <string>
#include <string_view>

#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"

namespace parsers::sci {

ParseResult<TokenNode<std::string_view>> ParseSimpleIdentNameNodeView(
    text::TextRange const& range, tokens::Token::Ident const& ident) {
  if (ident.trailer != tokens::Token::Ident::None) {
    return RangeFailureOf(range, "Expected simple identifier.");
  }
  return TokenNode<std::string_view>(ident.name, range);
}

ParseResult<TokenNode<std::string>> ParseSimpleIdentNameNode(
    text::TextRange const& range, tokens::Token::Ident const& ident) {
  if (ident.trailer != tokens::Token::Ident::None) {
    return RangeFailureOf(range, "Expected simple identifier.");
  }
  return TokenNode<std::string>(ident.name, range);
}

ParseResult<TokenNode<std::string_view>> ParseOneIdentTokenView(
    TreeExprSpan& exprs) {
  return ParseOneIdentToken(ParseSimpleIdentNameNodeView)(exprs);
}

ParseResult<TokenNode<std::string>> ParseOneIdentTokenNode(
    TreeExprSpan& exprs) {
  return ParseOneIdentToken(ParseSimpleIdentNameNode)(exprs);
}

ParseResult<TokenNode<int>> ParseOneNumberToken(TreeExprSpan& exprs) {
  return ParseOneTreeExpr(
      ParseTokenExpr(ParseNumToken([](text::TextRange const& range, int num) {
        return TokenNode<int>(num, range);
      })))(exprs);
}
}  // namespace parsers::sci