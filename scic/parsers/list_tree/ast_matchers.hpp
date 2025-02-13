#ifndef PARSERS_LIST_TREE_AST_MATCHERS_HPP
#define PARSERS_LIST_TREE_AST_MATCHERS_HPP

#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/tokens/token.hpp"
#include "util/choice_matchers.hpp"

namespace parsers::list_tree {

testing::Matcher<Expr const&> TokenExprOf(
    testing::Matcher<tokens::Token const&> const& token) {
  return util::ChoiceOf<TokenExpr>(
      testing::Property("token", &TokenExpr::token, token));
}

testing::Matcher<Expr const&> ListExprOf(
    testing::Matcher<absl::Span<Expr const>> const& token) {
  return util::ChoiceOf<ListExpr>(
      testing::Property("elements", &ListExpr::elements, token));
}

}  // namespace parsers::list_tree

#endif