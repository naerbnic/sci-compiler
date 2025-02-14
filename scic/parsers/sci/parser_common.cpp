#include "scic/parsers/sci/parser_common.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"

namespace parsers::sci {

std::optional<text::TextRange> TryParsePunct(tokens::Token::PunctType type,
                                             TreeExprSpan& exprs) {
  auto result = ParseOneTreeExpr(ParseTokenExpr(
      [type](
          list_tree::TokenExpr const& token) -> ParseResult<text::TextRange> {
        auto* punct = token.token().AsPunct();
        if (!punct || punct->type != type) {
          return RangeFailureOf(token.text_range(), "Expected punctuation.");
        }
        return token.text_range();
      }))(exprs);
  if (!result.ok()) {
    return std::nullopt;
  }
  return std::move(result).value();
}

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
ParseResult<TokenNode<std::string>> ParseOneStringToken(TreeExprSpan& exprs) {
  return ParseOneTreeExpr(ParseTokenExpr(
      ParseStringToken([](text::TextRange const& range, std::string str) {
        return TokenNode<std::string>(std::move(str), range);
      })))(exprs);
}
}  // namespace parsers::sci