#ifndef PARSERS_LIST_TREE_AST_MATCHERS_HPP
#define PARSERS_LIST_TREE_AST_MATCHERS_HPP

#include <string_view>

#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_test_utils.hpp"
#include "util/types/choice_matchers.hpp"

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

testing::Matcher<Expr const&> IdentExprOf(
    testing::Matcher<std::string_view> const& ident) {
  return TokenExprOf(tokens::IdentTokenOf(ident));
}

testing::Matcher<Expr const&> NumExprOf(testing::Matcher<int> const& num) {
  return TokenExprOf(tokens::NumTokenOf(num));
}

testing::Matcher<Expr const&> StringExprOf(
    testing::Matcher<std::string_view> const& str) {
  return TokenExprOf(tokens::StringTokenOf(str));
}

testing::Matcher<Expr const&> PunctExprOf(
    testing::Matcher<tokens::Token::PunctType> const& punct) {
  return TokenExprOf(tokens::PunctTokenOf(punct));
}

}  // namespace parsers::list_tree

#endif