#ifndef PARSERS_SCI_PARSER_COMMON_HPP
#define PARSERS_SCI_PARSER_COMMON_HPP

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

using TreeExpr = list_tree::Expr;
using TreeExprSpan = absl::Span<TreeExpr const>;

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
    if (!expr.has<list_tree::ListExpr>()) {
      return RangeFailureOf(
          expr.as<list_tree::TokenExpr>().token().text_range(),
          "Expected list.");
    }
    auto const& list_expr = expr.as<list_tree::ListExpr>();
    auto list_span = list_expr.elements();
    return parser(list_span);
  };
}

template <IsParserOf<list_tree::TokenExpr const&> F>
auto ParseTokenExpr(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser =
              std::move(parser)](TreeExpr const& expr) -> ParserInfo::ParseRet {
    if (!expr.has<list_tree::TokenExpr>()) {
      return RangeFailureOf(
          expr.as<list_tree::TokenExpr>().token().text_range(),
          "Expected list.");
    }
    return parser(expr.as<list_tree::TokenExpr>());
  };
}

template <IsParserOf<text::TextRange const&, tokens::Token::Ident const&> F>
auto ParseIdentToken(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser = std::move(parser)](
             list_tree::TokenExpr const& token) -> ParserInfo::ParseRet {
    auto* ident = token.token().AsIdent();
    if (!ident) {
      return RangeFailureOf(token.text_range(), "Expected identifier token.");
    }

    return parser(token.text_range(), *ident);
  };
}

template <IsParserOf<text::TextRange const&, int> F>
auto ParseNumToken(F parser) {
  using ParserInfo = ParserTraits<F>;
  return [parser = std::move(parser)](
             list_tree::TokenExpr const& token) -> ParserInfo::ParseRet {
    auto* num = token.token().AsNumber();
    if (!num) {
      return RangeFailureOf(token.text_range(), "Expected identifier token.");
    }

    return parser(token.text_range(), num->value);
  };
}

template <IsParserOf<text::TextRange const&, tokens::Token::Ident const&> F>
auto ParseOneIdentToken(F parser) {
  return ParseOneTreeExpr(ParseTokenExpr(ParseIdentToken(std::move(parser))));
}

ParseResult<TokenNode<std::string_view>> ParseSimpleIdentNameNodeView(
    text::TextRange const& range, tokens::Token::Ident const& ident);

ParseResult<TokenNode<std::string>> ParseSimpleIdentNameNode(
    text::TextRange const& range, tokens::Token::Ident const& ident);

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

// Read a standard identifier from the front of the expr stream with a
// string_view
ParseResult<TokenNode<std::string_view>> ParseOneIdentTokenView(
    TreeExprSpan& exprs);

// Read a standard identifier from the front of the expr stream with a
// string
ParseResult<TokenNode<std::string>> ParseOneIdentTokenNode(TreeExprSpan& exprs);

// Read a number from the front of the expr stream.
ParseResult<TokenNode<int>> ParseOneNumberToken(TreeExprSpan& exprs);

inline auto ParseOneLiteralIdent(std::string_view ident) {
  return
      [ident](TreeExprSpan& exprs) -> ParseResult<TokenNode<std::string_view>> {
        ASSIGN_OR_RETURN(auto token, ParseOneIdentTokenView(exprs));
        if (token.value() != ident) {
          return RangeFailureOf(token.text_range(), "Expected '%s'.", ident);
        }
        return token;
      };
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

template <class Pred, IsParserOf<TreeExpr const&> F>
auto ParseIf(Pred pred, F parser) {
  using ParserInfo = ParserTraits<F>;
  using ResultElem = ParserInfo::BaseRet;
  return [pred = std::move(pred), parser = std::move(parser)](
             TreeExprSpan& exprs) -> ParseResult<std::optional<ResultElem>> {
    // Make sure the pred is using a constant span.
    TreeExprSpan const pred_span = exprs;
    if (!pred(pred_span)) {
      return std::nullopt;
    }
    return parser(exprs);
  };
}

// A predicate that returns true if the input stream is empty, or if the first
// element fits the given predicate.
template <class Pred>
auto StartsWith(Pred pred) {
  return [pred = std::move(pred)](TreeExprSpan const& exprs) -> bool {
    return !exprs.empty() && pred(exprs.front());
  };
}

// A predicate that returns true if the input TreeExpr is a TokenExpr and the
// token fits the given predicate.
template <class Pred>
auto IsTokenExprWith(Pred pred) {
  return [pred = std::move(pred)](TreeExpr const& expr) -> bool {
    if (!expr.has<list_tree::TokenExpr>()) {
      return false;
    }
    return pred(expr.as<list_tree::TokenExpr>().token());
  };
}

inline auto IsIdentTokenWith(std::string_view ident) {
  return [ident](tokens::Token const& token) -> bool {
    auto* ident_ptr = token.AsIdent();
    if (!ident_ptr) {
      return false;
    }
    return ident_ptr->name == ident &&
           ident_ptr->trailer == tokens::Token::Ident::None;
  };
}

inline auto IsIdentExprWith(std::string_view ident) {
  return IsTokenExprWith(IsIdentTokenWith(ident));
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

}  // namespace parsers::sci

#endif