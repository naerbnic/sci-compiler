#include "scic/parsers/sci/parser.hpp"

#include <vector>

#include "absl/types/span.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/item_parsers.hpp"
#include "scic/parsers/sci/parser_common.hpp"

namespace parsers::sci {
using TreeExpr = list_tree::Expr;
using ::parsers::list_tree::ListExpr;
using ::parsers::list_tree::TokenExpr;

ParseResult<std::vector<Item>> ParseItems(
    absl::Span<TreeExpr const> exprs) {
  auto exprs_span = absl::MakeConstSpan(exprs);

  return ParseEachTreeExpr(ParseListExpr(ParseItem))(exprs_span);
}

}  // namespace parsers::sci