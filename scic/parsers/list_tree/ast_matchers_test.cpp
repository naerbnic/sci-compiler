#include "scic/parsers/list_tree/ast_matchers.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/list_tree/parser.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_readers.hpp"
#include "scic/tokens/token_test_utils.hpp"

namespace parsers::list_tree {
namespace {

using testing::_;
using testing::Not;
using testing::SizeIs;
using tokens::Token;

std::vector<Expr> ParseExprsOrDie(std::string_view text) {
  auto tokens =
      tokens::TokenizeText(text::TextRange::OfString(std::string(text)))
          .value();
  Parser parser(IncludeContext::GetEmpty());
  return parser.ParseTree(std::move(tokens)).value();
}

Expr ParseExprOrDie(std::string_view text) {
  auto exprs = ParseExprsOrDie(text);
  if (exprs.size() != 1) {
    throw std::runtime_error("Expected a single expression.");
  }
  return std::move(exprs[0]);
}

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