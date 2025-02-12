#include "scic/parsers/sci/parser.hpp"

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/combinators/status.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "util/status/status_macros.hpp"

namespace parsers::sci {
namespace {

using TreeExpr = list_tree::Expr;
using ::parsers::list_tree::ListExpr;
using ::parsers::list_tree::TokenExpr;

using TreeExprSpan = absl::Span<list_tree::Expr const>;

template <class F, class... Args>
concept CallableParser = requires(F const& parser, Args&&... args) {
  { parser(std::forward<Args>(args)...) };
  typename WrapParseResult<decltype(parser(std::forward<Args>(args)...))>;
};

template <class... Args>
ParseStatus FailureOf(absl::FormatSpec<Args...> const& spec,
                      Args const&... args) {
  return ParseStatus::Failure({diag::Diagnostic::Error(spec, args...)});
}
template <class... Args>
ParseStatus RangeFailureOf(text::TextRange const& range,
                           absl::FormatSpec<Args...> const& spec,
                           Args const&... args) {
  return ParseStatus::Failure(
      {diag::Diagnostic::RangeError(range, spec, args...)});
}

template <class F>
  requires CallableParser<F, TreeExpr const&>
auto ParseOneTreeExpr(F parser) {
  return [parser = std::move(parser)](
             TreeExprSpan& exprs) -> ParseResultOf<F const&, TreeExprSpan&> {
    if (exprs.empty()) {
      return FailureOf("Expected item.");
    }
    TreeExpr const& front_expr = exprs.front();
    auto result = parser(front_expr);
    if (result.ok()) {
      exprs.remove_prefix(1);
    }
    return result;
  };
}

template <class F>
  requires CallableParser<F, TreeExprSpan&>
auto ParseListExpr(F parser) {
  return [parser = std::move(parser)](
             TreeExpr const& expr) -> ParseResultOf<F const&, TreeExprSpan&> {
    if (!expr.has<ListExpr>()) {
      return RangeFailureOf(expr.as<TokenExpr>().token().text_range(),
                            "Expected list.");
    }
    auto const& list_expr = expr.as<ListExpr>();
    auto list_span = list_expr.elements();
    return parser(list_span);
  };
}

template <class F>
  requires CallableParser<F, TokenExpr const&>
auto ParseTokenExpr(F parser) {
  return
      [parser = std::move(parser)](
          TreeExpr const& expr) -> ParseResultOf<F const&, TokenExpr const&> {
        if (!expr.has<TokenExpr>()) {
          return RangeFailureOf(expr.as<TokenExpr>().token().text_range(),
                                "Expected list.");
        }
        return parser(expr.as<TokenExpr>());
      };
}

template <class F>
  requires CallableParser<F, text::TextRange const&,
                          tokens::Token::Ident const&>
auto ParseIdentToken(F parser) {
  return [parser = std::move(parser)](TokenExpr const& token)
             -> ParseResultOf<F const&, text::TextRange const&,
                              tokens::Token::Ident const&> {
    auto* ident = token.token().AsIdent();
    if (!ident) {
      return RangeFailureOf(token.text_range(), "Expected identifier token.");
    }

    return parser(token.text_range(), *ident);
  };
}

template <class F>
  requires CallableParser<F, text::TextRange const&,
                          tokens::Token::Ident const&>
auto ParseOneIdentToken(F parser) {
  return ParseTokenExpr(ParseIdentToken(std::move(parser)));
}

ParseResult<TokenNode<std::string_view>> ParseSimpleIdentNameNodeView(
    text::TextRange const& range, tokens::Token::Ident const& ident) {
  if (ident.trailer != tokens::Token::Ident::None) {
    return RangeFailureOf(range, "Expected simple identifier.");
  }
  return TokenNode<std::string_view>(ident.name, range);
}

template <class F>
auto ParseComplete(F parser) {
  return [parser = std::move(parser)](
             TreeExprSpan& exprs) -> ParseResultOf<F const&, TreeExprSpan&> {
    auto result = parser(exprs);
    if (result.ok() && !exprs.empty()) {
      return FailureOf("Unexpected trailing elements in list.");
    }
    return result;
  };
}

template <class F>
auto ParseOneListItem(TreeExprSpan& exprs, F&& parser)
    -> std::invoke_result_t<F&&, TreeExprSpan&> {
  return ParseOneTreeExpr(
      ParseListExpr(ParseComplete(std::forward<F>(parser))))(exprs);
}

ParseResult<TokenNode<std::string_view>> ParseOneIdentTokenView(
    TreeExprSpan& exprs) {
  return ParseOneTreeExpr(
      ParseTokenExpr(ParseIdentToken(ParseSimpleIdentNameNodeView)))(exprs);
}

template <class F>
  requires CallableParser<F, TreeExprSpan&>
auto ParseUntilComplete(F parser) {
  return [parser = std::move(parser)](TreeExprSpan& exprs)
             -> ParseResult<
                 std::vector<ParseResultElemOf<F const&, TreeExprSpan&>>> {
    using ResultElem = ParseResultElemOf<F const&, TreeExprSpan&>;
    std::vector<ResultElem> results;
    while (!exprs.empty()) {
      // Sanity check that at least one element gets consumed on success.
      auto exprs_copy = exprs;
      ASSIGN_OR_RETURN(ResultElem elem, parser(exprs));
      if (exprs_copy.size() == exprs.size() &&
          exprs_copy.data() == exprs.data()) {
        throw std::runtime_error(
            "Inner parser must consume at least one element.");
      }
      results.push_back(std::move(elem));
    }
    return results;
  };
}

// Parses a sequence of expressions, applying the given parser to each.
//
// We will attempt to parse each expression in the input list. If there are
// any errors, the result will be the combination of all of the errors.
template <class F>
  requires CallableParser<F, TreeExpr const&>
auto ParseEachTreeExpr(F parser) {
  return [parser = std::move(parser)](TreeExprSpan& exprs)
             -> ParseResult<
                 std::vector<ParseResultElemOf<F const&, TreeExpr const&>>> {
    std::optional<ParseStatus> curr_error;
    std::vector<ParseResultElemOf<F const&, TreeExpr const&>> results;
    for (auto const& expr : exprs) {
      auto result = parser(expr);
      if (!result.ok()) {
        // Don't store excess data
        results.clear();
        if (!curr_error) {
          curr_error = std::move(result).status();
        } else {
          curr_error =
              std::move(curr_error).value() | std::move(result).status();
        }
      } else if (!curr_error) {
        results.push_back(std::move(result).value());
      }
    }
    if (curr_error) {
      return std::move(curr_error).value();
    }
    return results;
  };
}
ParseResult<Item> ParsePublicItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenView(exprs));
  return Item(PublicDef({}));
}

ParseResult<Item> UnimplementedParseItem(
    TokenNode<std::string_view> const& keyword, TreeExprSpan& exprs) {
  return FailureOf("Unimplemented keyword: %s (%s)", keyword.value(),
                   keyword.text_range().GetRange().filename());
}

using ItemParser = std::function<ParseResult<Item>(
    TokenNode<std::string_view> const& keyword, TreeExprSpan&)>;
using ParserItemMap = std::map<std::string, ItemParser>;

ParserItemMap const& TopLevelParsers() {
  static ParserItemMap* instance = new ParserItemMap(([] {
    return ParserItemMap({
        {"public", ParsePublicItem},
    });
  })());
  return *instance;
}

ItemParser const& GetItemParser(std::string_view keyword) {
  auto const& parsers = TopLevelParsers();
  auto it = parsers.find(std::string(keyword));
  if (it == parsers.end()) {
    static ItemParser const unimplemented_parser(UnimplementedParseItem);
    return unimplemented_parser;
  }
  return it->second;
}

ParseResult<Item> ParseItem(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenView(exprs));
  return GetItemParser(name.value())(name, exprs);
}

}  // namespace

ParseResult<std::vector<Item>> ParseItems(
    std::vector<list_tree::Expr> const& exprs) {
  auto exprs_span = absl::MakeConstSpan(exprs);

  return ParseEachTreeExpr(ParseListExpr(ParseItem))(exprs_span);
}

}  // namespace parsers::sci