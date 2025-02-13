#ifndef PARSERS_LIST_TREE_PARSER_TEST_UTILS_HPP
#define PARSERS_LIST_TREE_PARSER_TEST_UTILS_HPP

#include <string_view>
#include <vector>

#include "scic/parsers/list_tree/ast.hpp"

namespace parsers::list_tree {

// Parses a string containing a sequence of list_tree expressions and returns
// the parsed expressions.  If the string cannot be parsed, this function will
// throw an exception.
std::vector<Expr> ParseExprsOrDie(std::string_view text);

// Parses a string containing a list_tree expression and returns
// the parsed expressions.  If the string cannot be parsed into a single Expr,
// this function will throw an exception.
Expr ParseExprOrDie(std::string_view text);

}  // namespace parsers::list_tree
#endif