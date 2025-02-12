#include "scic/parsers/sci/parser.hpp"

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/parse_func.hpp"
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

// Parses a single tree expression from the input stream. On error, the
// stream will not be affected.
template <IsParserOf<TreeExpr const&> F>
auto ParseOneTreeExpr(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser =
              std::move(parser)](TreeExprSpan& exprs) -> ParserInfo::ParseRet {
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

// Parses a single list tree expression from the input span, applying the
// given parser to the list contents.
template <IsParserOf<TreeExprSpan&> F>
auto ParseListExpr(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser =
              std::move(parser)](TreeExpr const& expr) -> ParserInfo::ParseRet {
    if (!expr.has<ListExpr>()) {
      return RangeFailureOf(expr.as<TokenExpr>().token().text_range(),
                            "Expected list.");
    }
    auto const& list_expr = expr.as<ListExpr>();
    auto list_span = list_expr.elements();
    return parser(list_span);
  };
}

template <IsParserOf<TokenExpr const&> F>
auto ParseTokenExpr(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser =
              std::move(parser)](TreeExpr const& expr) -> ParserInfo::ParseRet {
    if (!expr.has<TokenExpr>()) {
      return RangeFailureOf(expr.as<TokenExpr>().token().text_range(),
                            "Expected list.");
    }
    return parser(expr.as<TokenExpr>());
  };
}

template <IsParserOf<text::TextRange const&, tokens::Token::Ident const&> F>
auto ParseIdentToken(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser = std::move(parser)](
             TokenExpr const& token) -> ParserInfo::ParseRet {
    auto* ident = token.token().AsIdent();
    if (!ident) {
      return RangeFailureOf(token.text_range(), "Expected identifier token.");
    }

    return parser(token.text_range(), *ident);
  };
}

template <IsParserOf<text::TextRange const&, tokens::Token::Ident const&> F>
auto ParseOneIdentToken(F parser) {
  return ParseOneTreeExpr(ParseTokenExpr(ParseIdentToken(std::move(parser))));
}

ParseResult<TokenNode<std::string_view>> ParseSimpleIdentNameNodeView(
    text::TextRange const& range, tokens::Token::Ident const& ident) {
  if (ident.trailer != tokens::Token::Ident::None) {
    return RangeFailureOf(range, "Expected simple identifier.");
  }
  return TokenNode<std::string_view>(ident.name, range);
}

// Ensures that a span parser consumes all elements in the input list.
template <IsParserOf<TreeExprSpan&> F>
auto ParseComplete(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser =
              std::move(parser)](TreeExprSpan& exprs) -> ParserInfo::ParseRet {
    auto result = parser(exprs);
    if (result.ok() && !exprs.empty()) {
      return FailureOf("Unexpected trailing elements in list.");
    }
    return result;
  };
}

template <IsParserOf<TreeExprSpan&> F>
auto ParseOneListItem(F parser) {
  return ParseOneTreeExpr(
      ParseListExpr(ParseComplete(std::forward<F>(parser))));
}

ParseResult<TokenNode<std::string_view>> ParseOneIdentTokenView(
    TreeExprSpan& exprs) {
  static auto const instance =
      ParseFunc(ParseOneIdentToken(ParseSimpleIdentNameNodeView));
  return instance(exprs);
}

template <IsParserOf<TreeExprSpan&> F>
auto ParseUntilComplete(F parser) {
  using ParserInfo = ParserTraits<F>;
  using ResultElem = ParserInfo::BaseRet;
  return [parser = std::move(parser)](
             TreeExprSpan& exprs) -> ParseResult<std::vector<ResultElem>> {
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
template <IsParserOf<TreeExpr const&> F>
auto ParseEachTreeExpr(F parser) {
  using ParserInfo = ParserTraits<F>;
  using ResultElem = ParserInfo::BaseRet;
  return [parser = std::move(parser)](
             TreeExprSpan& exprs) -> ParseResult<std::vector<ResultElem>> {
    std::optional<ParseStatus> curr_error;
    std::vector<ResultElem> results;
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

using ItemParser =
    ParseFunc<Item(TokenNode<std::string_view> const& keyword, TreeExprSpan&)>;
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