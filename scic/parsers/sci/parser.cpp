#include "scic/parsers/sci/parser.hpp"

#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/combinators.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"

namespace parsers::sci {
namespace {

using TreeExpr = list_tree::Expr;
using ::parsers::list_tree::ListExpr;
using ::parsers::list_tree::TokenExpr;

using TreeExprSpan = absl::Span<list_tree::Expr const>;

template <class F>
auto ParseOneListItem(TreeExprSpan& exprs, F&& parser)
    -> std::invoke_result_t<F&&, TreeExprSpan&> {
  if (exprs.empty()) {
    return ParseError::Failure({diag::Diagnostic::Error("Expected item.")});
  }
  auto const& front_expr = exprs.front();
  if (!front_expr.has<ListExpr>()) {
    return ParseError::Failure({diag::Diagnostic::RangeError(
        front_expr.as<TokenExpr>().text_range(), "Expected list.")});
  }

  auto elements = front_expr.as<ListExpr>().elements();
  auto result = parser(elements);
  if (result.ok() && !elements.empty()) {
    return ParseError::Failure(
        {diag::Diagnostic::Error("Unexpected trailing elements in list.")});
  }
  exprs.remove_prefix(1);
  return result;
}

ParseResult<Item> ParseItem(absl::Span<list_tree::Expr const> const& exprs) {
  return ParseError::Fatal({diag::Diagnostic::Error("Not implemented.")});
}

}  // namespace

ParseResult<std::vector<Item>> ParseItems(
    std::vector<list_tree::Expr> const& exprs) {
  auto exprs_span = absl::MakeConstSpan(exprs);

  auto result = ParseResult((std::vector<Item>()));
  while (!exprs_span.empty()) {
    result = (std::move(result) | ParseOneListItem(exprs_span, ParseItem))
                 .Apply([&](std::vector<Item> vec, Item item) {
                   vec.push_back(std::move(item));
                   return vec;
                 });
  }
  return result;
}

}  // namespace parsers::sci