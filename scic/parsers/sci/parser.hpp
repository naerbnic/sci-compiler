#ifndef PARSERS_SCI_PARSER_HPP
#define PARSERS_SCI_PARSER_HPP

#include <vector>

#include "scic/parsers/combinators/combinators.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"

namespace parsers::sci {

ParseResult<std::vector<Item>> ParseItems(
    std::vector<list_tree::Expr> const& exprs);

}  // namespace parsers::sci
#endif