#include "scic/parsers/list_tree/ast_matchers.hpp"

#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/parsers/list_tree/parser_test_utils.hpp"
#include "scic/tokens/token.hpp"

namespace parsers::list_tree {
namespace {

using testing::_;
using testing::Not;
using testing::SizeIs;
using tokens::Token;

TEST(TokenExprOfTest, Basic) {
  auto expr = ParseExprOrDie("foo");
  EXPECT_THAT(expr, TokenExprOf(_));
}

TEST(TokenExprOfTest, Invalid) {
  auto expr = ParseExprOrDie("(a b c)");
  EXPECT_THAT(expr, Not(TokenExprOf(_)));
}

TEST(ListExprOfTest, Basic) {
  auto expr = ParseExprOrDie("(a b c)");
  EXPECT_THAT(expr, ListExprOf(SizeIs(3)));
}

TEST(ListExprOfTest, Invalid) {
  auto expr = ParseExprOrDie("foo");
  EXPECT_THAT(expr, Not(ListExprOf(_)));
}

}  // namespace
}  // namespace parsers::list_tree