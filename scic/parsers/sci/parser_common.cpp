#include "scic/parsers/sci/parser_common.hpp"

#include <optional>
#include <string_view>
#include <utility>

#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_source.hpp"
#include "util/strings/ref_str.hpp"

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
    tokens::TokenSource const& source, tokens::Token::Ident const& ident) {
  if (ident.trailer != tokens::Token::Ident::None) {
    return RangeFailureOf(source.sources()[0], "Expected simple identifier.");
  }
  return TokenNode<std::string_view>(ident.name, source);
}

ParseResult<TokenNode<util::RefStr>> ParseSimpleIdentNameNode(
    tokens::TokenSource const& source, tokens::Token::Ident const& ident) {
  if (ident.trailer != tokens::Token::Ident::None) {
    return RangeFailureOf(source.sources()[0], "Expected simple identifier.");
  }
  return TokenNode<util::RefStr>(ident.name, source);
}

ParseResult<std::pair<TokenNode<util::RefStr>, tokens::Token::Ident::Trailer>>
ParseIdentNameNode(list_tree::TokenExpr const& token) {
  auto* ident = token.token().AsIdent();
  if (!ident) {
    return RangeFailureOf(token.text_range(), "Expected identifier token.");
  }
  return std::make_pair(
      TokenNode<util::RefStr>(ident->name, token.text_range()), ident->trailer);
}

ParseResult<TokenNode<std::string_view>> ParseOneIdentTokenView(
    TreeExprSpan& exprs) {
  return ParseOneIdentToken(ParseSimpleIdentNameNodeView)(exprs);
}

ParseResult<TokenNode<util::RefStr>> ParseOneIdentTokenNode(
    TreeExprSpan& exprs) {
  return ParseOneIdentToken(ParseSimpleIdentNameNode)(exprs);
}

ParseResult<TokenNode<int>> ParseOneNumberToken(TreeExprSpan& exprs) {
  return ParseOneTreeExpr(
      ParseTokenExpr(ParseNumToken([](tokens::TokenSource const& sources, int num) {
        return TokenNode<int>(num, sources);
      })))(exprs);
}
ParseResult<TokenNode<util::RefStr>> ParseOneStringToken(TreeExprSpan& exprs) {
  return ParseOneTreeExpr(ParseTokenExpr(
      ParseStringToken([](tokens::TokenSource const& sources, util::RefStr str) {
        return TokenNode<util::RefStr>(std::move(str), sources);
      })))(exprs);
}
}  // namespace parsers::sci