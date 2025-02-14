#include "scic/parsers/sci/expr_parsers.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "scic/parsers/combinators/parse_func.hpp"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"
#include "scic/tokens/token.hpp"
#include "util/status/status_macros.hpp"

namespace parsers::sci {
namespace {

using ExprParseFunc = ParseFunc<std::unique_ptr<Expr>(
    TokenNode<std::string_view> keyword, TreeExprSpan&)>;

using BuiltinsMap = std::map<std::string, ExprParseFunc>;

ParseResult<std::unique_ptr<Expr>> ParseExprPtr(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto expr, ParseExpr(exprs));
  return std::make_unique<Expr>(std::move(expr));
}

BuiltinsMap const& GetBuiltinParsers() {
  static const BuiltinsMap instance = {};
  return instance;
}

ParseResult<std::optional<SelectLitExpr>> ParseSelectLitExpr(
    TreeExprSpan& exprs) {
  auto ident_result = TryParsePunct(tokens::Token::PCT_HASH, exprs);
  if (!ident_result) {
    return std::nullopt;
  }

  ASSIGN_OR_RETURN(auto selector, ParseOneIdentTokenNode(exprs));
  return SelectLitExpr(std::move(selector));
}

ParseResult<std::optional<AddrOfExpr>> ParseAddrOfExpr(TreeExprSpan& exprs) {
  auto ident_result = TryParsePunct(tokens::Token::PCT_AT, exprs);
  if (!ident_result) {
    return std::nullopt;
  }

  ASSIGN_OR_RETURN(auto expr, ParseExprPtr(exprs));
  return AddrOfExpr(std::move(expr));
}

}  // namespace

ParseResult<ArrayIndexExpr> ParseArrayIndexExpr(TreeExprSpan const& exprs) {
  auto local_exprs = exprs;
  return ParseComplete([](TreeExprSpan& exprs) -> ParseResult<ArrayIndexExpr> {
    ASSIGN_OR_RETURN(auto array_name, ParseOneIdentTokenNode(exprs));
    ASSIGN_OR_RETURN(auto index_expr, ParseExprPtr(exprs));
    return ArrayIndexExpr(std::move(array_name), std::move(index_expr));
  })(local_exprs);
}

ParseResult<SendExpr> ParseSendExpr(TokenNode<std::string> target,
                                    TreeExprSpan& exprs) {}
ParseResult<SendExpr> ParseCall(TokenNode<std::string> target,
                                TreeExprSpan& exprs) {}

bool IsSelectorIdent(tokens::Token const& token) {
  auto ident = token.AsIdent();
  if (!ident) {
    return false;
  }
  return ident->trailer == tokens::Token::Ident::None;
}

ParseResult<Expr> ParseSciListExpr(TreeExprSpan const& exprs) {
  auto local_exprs = exprs;
  ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenNode(local_exprs));

  // There are three possibilities here:
  //
  // - The expression is a builtin function call
  // - The expression is a function call
  // - The expression is a method send
  //
  // For the latter two,  we have to look at the other arguments to determine
  // which it is. If it starts with a selector call it's a method send,
  // otherwise it's a function call.

  auto const& builtins = GetBuiltinParsers();
  if (auto it = builtins.find(name.value()); it != builtins.end()) {
    return it->second(
        TokenNode<std::string_view>(name.value(), name.text_range()),
        local_exprs);
  }

  // This isn't a builtin, so we need to determine if it's a method send or a
  // function call. A send expression will be an identifier with either a
  // question mark or a colon after it.

  if (StartsWith(IsTokenExprWith(IsSelectorIdent))(local_exprs)) {
    return ParseSendExpr(std::move(name), local_exprs);
  } else {
    return ParseCall(std::move(name), local_exprs);
  }
}

ParseResult<Expr> ParseExpr(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto select_lit, ParseSelectLitExpr(exprs));
  if (select_lit) {
    return std::move(select_lit).value();
  }

  ASSIGN_OR_RETURN(auto addr_of, ParseAddrOfExpr(exprs));
  if (addr_of) {
    return std::move(addr_of).value();
  }

  // FIXME: Add logic for an &rest expression here.

  return ParseOneTreeExpr([](TreeExpr const& expr) -> ParseResult<Expr> {
    return expr.visit(
        [](list_tree::TokenExpr const& expr) {
          auto const& token = expr.token();
          return token.value().visit(
              [&](tokens::Token::Ident const& ident) -> ParseResult<Expr> {
                if (ident.trailer != tokens::Token::Ident::None) {
                  return RangeFailureOf(token.text_range(),
                                        "Expected simple identifier.");
                }
                return VarExpr(
                    TokenNode<std::string>(ident.name, token.text_range()));
              },
              [&](tokens::Token::Number const& num) -> ParseResult<Expr> {
                return ConstValueExpr(NumConstValue(
                    TokenNode<int>(num.value, token.text_range())));
              },
              [&](tokens::Token::String const& str) -> ParseResult<Expr> {
                return ConstValueExpr(StringConstValue(TokenNode<std::string>(
                    str.decodedString, token.text_range())));
              },
              [&](auto const&) -> ParseResult<Expr> {
                return RangeFailureOf(token.text_range(),
                                      "Unexpected token type.");
              });
        },
        [](list_tree::ListExpr const& expr) -> ParseResult<Expr> {
          switch (expr.kind()) {
            case list_tree::ListExpr::PARENS:
              return ParseSciListExpr(expr.elements());
            case list_tree::ListExpr::BRACKETS:
              return ParseArrayIndexExpr(expr.elements());
          }
        });
  })(exprs);
}

}  // namespace parsers::sci