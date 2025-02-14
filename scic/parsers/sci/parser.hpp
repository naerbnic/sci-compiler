#ifndef PARSERS_SCI_PARSER_HPP
#define PARSERS_SCI_PARSER_HPP

#include <vector>

#include "absl/types/span.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"

namespace parsers::sci {

ParseResult<std::vector<Item>> ParseItems(
    absl::Span<list_tree::Expr const> exprs);

}  // namespace parsers::sci
#endif