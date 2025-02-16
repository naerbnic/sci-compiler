#include "scic/parsers/sci/expr_parsers.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "scic/parsers/combinators/parse_func.hpp"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/const_value_parsers.hpp"
#include "scic/parsers/sci/parser_common.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"

namespace parsers::sci {

namespace {

using ExprParseFunc =
    ParseFunc<Expr(TokenNode<std::string_view> keyword, TreeExprSpan&)>;

using BuiltinsMap = std::map<util::RefStr, ExprParseFunc>;

ParseResult<std::unique_ptr<Expr>> ParseExprPtr(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto expr, ParseExpr(exprs));
  return std::make_unique<Expr>(std::move(expr));
}

// Builtin Parsers

ParseResult<ReturnExpr> ParseReturnExpr(TokenNode<std::string_view> keyword,
                                        TreeExprSpan& exprs) {
  std::optional<std::unique_ptr<Expr>> ret_value;
  if (!exprs.empty()) {
    ASSIGN_OR_RETURN(ret_value, ParseComplete(ParseExprPtr)(exprs));
  }
  return ReturnExpr(std::move(ret_value));
}

ParseResult<BreakExpr> ParseBreakExpr(TokenNode<std::string_view> keyword,
                                      TreeExprSpan& exprs) {
  std::optional<TokenNode<int>> index;
  if (!exprs.empty()) {
    ASSIGN_OR_RETURN(index, ParseOneNumberToken(exprs));
  }
  return BreakExpr(std::nullopt, std::move(index));
}

ParseResult<BreakExpr> ParseBreakIfExpr(TokenNode<std::string_view> keyword,
                                        TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto condition, ParseExprPtr(exprs));
  std::optional<TokenNode<int>> index;
  if (!exprs.empty()) {
    ASSIGN_OR_RETURN(index, ParseOneNumberToken(exprs));
  }
  return BreakExpr(std::move(condition), std::move(index));
}

ParseResult<ContinueExpr> ParseContinueExpr(TokenNode<std::string_view> keyword,
                                            TreeExprSpan& exprs) {
  std::optional<TokenNode<int>> index;
  if (!exprs.empty()) {
    ASSIGN_OR_RETURN(index, ParseOneNumberToken(exprs));
  }
  return ContinueExpr(std::nullopt, std::move(index));
}

ParseResult<ContinueExpr> ParseContIfExpr(TokenNode<std::string_view> keyword,
                                          TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto condition, ParseExprPtr(exprs));
  std::optional<TokenNode<int>> index;
  if (!exprs.empty()) {
    ASSIGN_OR_RETURN(index, ParseOneNumberToken(exprs));
  }
  return ContinueExpr(std::move(condition), std::move(index));
}

ParseResult<WhileExpr> ParseWhileExpr(TokenNode<std::string_view> keyword,
                                      TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto condition, ParseExprPtr(exprs));
  ASSIGN_OR_RETURN(auto body, ParseExprPtr(exprs));
  return WhileExpr(std::move(condition), std::move(body));
}

ParseResult<WhileExpr> ParseRepeatExpr(TokenNode<std::string_view> keyword,
                                       TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto body, ParseExprList(exprs));
  return WhileExpr(std::nullopt, std::make_unique<Expr>(std::move(body)));
}

ParseResult<ForExpr> ParseForExpr(TokenNode<std::string_view> keyword,
                                  TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto init, ParseOneListItem(ParseExprList)(exprs));
  ASSIGN_OR_RETURN(auto condition, ParseExprPtr(exprs));
  ASSIGN_OR_RETURN(auto update, ParseOneListItem(ParseExprList)(exprs));
  ASSIGN_OR_RETURN(auto body, ParseComplete(ParseExprList)(exprs));
  return ForExpr(std::make_unique<Expr>(std::move(init)), std::move(condition),
                 std::make_unique<Expr>(std::move(update)),
                 std::make_unique<Expr>(std::move(body)));
}

ParseResult<IfExpr> ParseIfExpr(TokenNode<std::string_view> keyword,
                                TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto condition, ParseExprPtr(exprs));
  ASSIGN_OR_RETURN(auto then_body, ParseExprPtr(exprs));
  std::optional<std::unique_ptr<Expr>> else_body;
  if (!exprs.empty()) {
    ASSIGN_OR_RETURN(else_body, ParseExprPtr(exprs));
  }
  return IfExpr(std::move(condition), std::move(then_body),
                std::move(else_body));
}

ParseResult<CondExpr> ParseCondExpr(TokenNode<std::string_view> keyword,
                                    TreeExprSpan& exprs) {
  // We create a separate branch clause type here to
  // track where we find "else" clauses.
  struct BranchClause {
    // The condition. If std::nullopt, this is an else clause.
    std::optional<std::unique_ptr<Expr>> condition;
    std::unique_ptr<Expr> body;
  };

  ASSIGN_OR_RETURN(
      auto branches,
      ParseUntilComplete(ParseOneTreeExpr(
          ParseListExpr([](TreeExprSpan& exprs) -> ParseResult<BranchClause> {
            if (exprs.empty()) {
              return FailureOf("Expected condition expression.");
            }
            std::optional<std::unique_ptr<Expr>> condition;
            if (StartsWith(IsIdentExprWith("else"))(exprs)) {
              ASSIGN_OR_RETURN(auto else_token, ParseOneIdentTokenNode(exprs));
            } else {
              ASSIGN_OR_RETURN(condition, ParseExprPtr(exprs));
            }

            ASSIGN_OR_RETURN(auto body, ParseExprList(exprs));

            return BranchClause{
                .condition = std::move(condition),
                .body = std::make_unique<Expr>(std::move(body)),
            };
          })))(exprs));

  std::optional<std::unique_ptr<Expr>> else_body;
  if (branches.size() > 0 && !branches.back().condition) {
    // The last entry is an else clause. Pop it off, and set the else body.
    else_body = std::move(branches.back().body);
    branches.pop_back();
  }

  std::vector<CondExpr::Branch> branch_ast;
  for (auto& branch : branches) {
    if (!branch.condition) {
      return RangeFailureOf(keyword.text_range(),
                            "Got an else in the middle of the cond expr.");
    }
    branch_ast.push_back(CondExpr::Branch{
        .condition = std::move(branch.condition).value(),
        .body = std::move(branch.body),
    });
  }

  return CondExpr(std::move(branch_ast), std::move(else_body));
}

ParseResult<SwitchExpr> ParseSwitchExpr(TokenNode<std::string_view> keyword,
                                        TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto switch_expr, ParseExprPtr(exprs));
  // We create a separate branch clause type here to
  // track where we find "else" clauses.
  struct CaseClause {
    // The condition. If std::nullopt, this is an else clause.
    std::optional<ConstValue> condition;
    std::unique_ptr<Expr> body;
  };

  ASSIGN_OR_RETURN(
      auto case_clauses,
      ParseUntilComplete(ParseOneTreeExpr(
          ParseListExpr([](TreeExprSpan& exprs) -> ParseResult<CaseClause> {
            if (exprs.empty()) {
              return FailureOf("Expected case value.");
            }
            std::optional<ConstValue> condition;
            if (StartsWith(IsIdentExprWith("else"))(exprs)) {
              ASSIGN_OR_RETURN(auto else_token, ParseOneIdentTokenNode(exprs));
            } else {
              ASSIGN_OR_RETURN(condition, ParseOneConstValue(exprs));
            }

            ASSIGN_OR_RETURN(auto body, ParseExprList(exprs));
            return CaseClause{
                .condition = std::move(condition),
                .body = std::make_unique<Expr>(std::move(body)),
            };
          })))(exprs));

  std::optional<std::unique_ptr<Expr>> else_body;
  if (case_clauses.size() > 0 && !case_clauses.back().condition) {
    // The last entry is an else clause. Pop it off, and set the else body.
    else_body = std::move(case_clauses.back().body);
    case_clauses.pop_back();
  }
  std::vector<SwitchExpr::Case> cases;
  for (auto& case_clause : case_clauses) {
    if (!case_clause.condition) {
      return RangeFailureOf(keyword.text_range(),
                            "Got an else in the middle of the switch expr.");
    }
    cases.push_back(SwitchExpr::Case{
        .value = std::move(case_clause.condition).value(),
        .body = std::move(case_clause.body),
    });
  }

  return SwitchExpr(std::move(switch_expr), std::move(cases),
                    std::move(else_body));
}

ParseResult<SwitchToExpr> ParseSwitchToExpr(TokenNode<std::string_view> keyword,
                                            TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto switch_expr, ParseExprPtr(exprs));
  // We create a separate branch clause type here to
  // track where we find "else" clauses.
  struct CaseClause {
    bool is_else;
    std::unique_ptr<Expr> body;
  };

  ASSIGN_OR_RETURN(
      auto case_clauses,
      ParseUntilComplete(ParseOneTreeExpr(
          ParseListExpr([](TreeExprSpan& exprs) -> ParseResult<CaseClause> {
            bool is_else = false;
            if (StartsWith(IsIdentExprWith("else"))(exprs)) {
              ASSIGN_OR_RETURN(auto else_token, ParseOneIdentTokenNode(exprs));
              is_else = true;
            }

            ASSIGN_OR_RETURN(auto body, ParseExprList(exprs));
            return CaseClause{
                .is_else = is_else,
                .body = std::make_unique<Expr>(std::move(body)),
            };
          })))(exprs));

  std::optional<std::unique_ptr<Expr>> else_body;
  if (case_clauses.size() > 0 && case_clauses.back().is_else) {
    // The last entry is an else clause. Pop it off, and set the else body.
    else_body = std::move(case_clauses.back().body);
    case_clauses.pop_back();
  }
  std::vector<std::unique_ptr<Expr>> cases;
  for (auto& case_clause : case_clauses) {
    if (case_clause.is_else) {
      return RangeFailureOf(keyword.text_range(),
                            "Got an else in the middle of the switch expr.");
    }
    cases.push_back(std::move(case_clause.body));
  }

  return SwitchToExpr(std::move(switch_expr), std::move(cases),
                      std::move(else_body));
}

ParseResult<SendExpr> ParseSelfSendExpr(TokenNode<std::string_view> keyword,
                                        TreeExprSpan& exprs) {
  return ParseSendExpr(SelfSendTarget(), exprs);
}
ParseResult<SendExpr> ParseSuperSendExpr(TokenNode<std::string_view> keyword,
                                         TreeExprSpan& exprs) {
  return ParseSendExpr(SuperSendTarget(), exprs);
}

template <AssignExpr::Kind K>
ParseResult<AssignExpr> ParseAssignExpr(TokenNode<std::string_view> keyword,
                                        TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto var, ParseExprPtr(exprs));
  ASSIGN_OR_RETURN(auto value, ParseExprPtr(exprs));
  return AssignExpr(K, std::move(var), std::move(value));
}

BuiltinsMap const& GetBuiltinParsers() {
  static const BuiltinsMap instance = {
      {"return", ParseReturnExpr},
      {"break", ParseBreakExpr},
      {"breakif", ParseBreakIfExpr},
      {"continue", ParseContinueExpr},
      {"contif", ParseContIfExpr},
      {"while", ParseWhileExpr},
      {"repeat", ParseRepeatExpr},
      {"for", ParseForExpr},
      {"if", ParseIfExpr},
      {"cond", ParseCondExpr},
      {"switch", ParseSwitchExpr},
      {"switchto", ParseSwitchToExpr},
      {"self", ParseSelfSendExpr},
      {"super", ParseSuperSendExpr},
      {"=", ParseAssignExpr<AssignExpr::Kind::DIRECT>},
      {"+=", ParseAssignExpr<AssignExpr::Kind::ADD>},
      {"-=", ParseAssignExpr<AssignExpr::Kind::SUB>},
      {"*=", ParseAssignExpr<AssignExpr::Kind::MUL>},
      {"/=", ParseAssignExpr<AssignExpr::Kind::DIV>},
      {"mod=", ParseAssignExpr<AssignExpr::Kind::MOD>},
      {"&=", ParseAssignExpr<AssignExpr::Kind::AND>},
      {"|=", ParseAssignExpr<AssignExpr::Kind::OR>},
      {"^=", ParseAssignExpr<AssignExpr::Kind::XOR>},
      {">>=", ParseAssignExpr<AssignExpr::Kind::SHR>},
      {"<<=", ParseAssignExpr<AssignExpr::Kind::SHL>},
  };
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

bool IsSelectorIdent(tokens::Token const& token) {
  auto ident = token.AsIdent();
  if (!ident) {
    return false;
  }
  return ident->trailer != tokens::Token::Ident::None;
}

ParseResult<CallArgs> ParseCallArgs(TreeExprSpan& args) {
  std::vector<Expr> arg_exprs;
  std::optional<CallArgs::Rest> rest_expr;
  while (!args.empty()) {
    if (StartsWith(IsIdentExprWith("&rest"))(args)) {
      ASSIGN_OR_RETURN(auto rest_token, ParseOneIdentTokenView(args));
      std::optional<TokenNode<util::RefStr>> rest_var;
      if (!args.empty()) {
        ASSIGN_OR_RETURN(rest_var, ParseOneIdentTokenNode(args));
      }

      if (!args.empty()) {
        return RangeFailureOf(rest_token.text_range(),
                              "Expected no more arguments after &rest clause.");
      }

      break;
    }

    ASSIGN_OR_RETURN(auto arg_expr, ParseExpr(args));
    arg_exprs.push_back(std::move(arg_expr));
  }

  return CallArgs(std::move(arg_exprs), std::move(rest_expr));
}

ParseResult<SendClause> ParseSendClause(TreeExprSpan& exprs) {
  return ParseOrRestore<TreeExpr const>(
      [](TreeExprSpan& exprs) -> ParseResult<SendClause> {
        ASSIGN_OR_RETURN(
            auto selector,
            ParseOneTreeExpr(ParseTokenExpr(ParseIdentNameNode))(exprs));
        auto [name, trailer] = std::move(selector);
        if (trailer == tokens::Token::Ident::None) {
          return RangeFailureOf(
              name.text_range(),
              "Expected selector ending in '?' or ':' in send clause.");
        }

        auto clause_end =
            std::ranges::find_if(exprs, IsTokenExprWith(IsSelectorIdent));

        TreeExprSpan args = exprs.subspan(0, clause_end - exprs.begin());
        // FIXME: This can cause the parse span to change on failure.
        exprs = exprs.subspan(clause_end - exprs.begin());

        if (trailer == tokens::Token::Ident::Question) {
          if (!args.empty()) {
            return RangeFailureOf(
                name.text_range(),
                "Getter selectors (ending in '?') should not have arguments.");
          }

          return PropReadSendClause(std::move(name));
        }

        ASSIGN_OR_RETURN(auto call_args, ParseCallArgs(args));

        return MethodSendClause(std::move(name), std::move(call_args));
      })(exprs);
}

ParseResult<SendExpr> ParseSendExpr(SendTarget target, TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto clauses, ParseUntilComplete(ParseSendClause)(exprs));

  return SendExpr(std::move(target), std::move(clauses));
}

ParseResult<CallExpr> ParseCall(Expr target, TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto args, ParseCallArgs(exprs));

  return CallExpr(std::make_unique<Expr>(std::move(target)), std::move(args));
}

ParseResult<Expr> ParseSciListExpr(TreeExprSpan const& exprs) {
  // There are three possibilities here:
  //
  // - The expression is a builtin function call
  // - The expression is a function call
  // - The expression is a method send
  //
  // For the latter two,  we have to look at the other arguments to determine
  // which it is. If it starts with a selector call it's a method send,
  // otherwise it's a function call.

  auto local_exprs = exprs;
  std::optional<Expr> target_expr;
  if (StartsWith(IsIdentExpr)(local_exprs)) {
    ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenNode(local_exprs));

    auto const& builtins = GetBuiltinParsers();
    if (auto it = builtins.find(name.value()); it != builtins.end()) {
      return it->second(
          TokenNode<std::string_view>(name.value(), name.text_range()),
          local_exprs);
    }
    target_expr = VarExpr(std::move(name));
  } else {
    ASSIGN_OR_RETURN(target_expr, ParseExpr(local_exprs));
  }

  // This isn't a builtin, so we need to determine if it's a method send or a
  // function call. The start of a send expression will be an identifier with
  // either a question mark or a colon after it, which will be its selector.

  if (StartsWith(IsTokenExprWith(IsSelectorIdent))(local_exprs)) {
    return ParseSendExpr(
        ExprSendTarget(std::make_unique<Expr>(std::move(target_expr).value())),
        local_exprs);
  } else {
    return ParseCall(std::move(target_expr).value(), local_exprs);
  }
}

ParseResult<ExprList> ParseExprList(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto expr_list, ParseUntilComplete(ParseExpr)(exprs));
  return ExprList(std::move(expr_list));
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

  // The original code has handling for a free-floating &rest expression,
  // but it appears it can only show up in procedure calls and sends.

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
                    TokenNode<util::RefStr>(ident.name, token.text_range()));
              },
              [&](tokens::Token::Number const& num) -> ParseResult<Expr> {
                return ConstValueExpr(NumConstValue(
                    TokenNode<int>(num.value, token.text_range())));
              },
              [&](tokens::Token::String const& str) -> ParseResult<Expr> {
                return ConstValueExpr(StringConstValue(TokenNode<util::RefStr>(
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