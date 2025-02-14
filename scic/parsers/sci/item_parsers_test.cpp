#include "scic/parsers/sci/item_parsers.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "absl/types/span.h"
#include "scic/parsers/list_tree/parser_test_utils.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/text/text_range.hpp"

namespace parsers::sci {
namespace {

TokenNode<std::string_view> StringViewTokenNode(std::string_view value) {
  return TokenNode<std::string_view>(
      value, text::TextRange::OfString(std::string(value)));
}

TEST(PublicTest, Unimplemented) {
  auto exprs = list_tree::ParseExprsOrDie("a 1 b 2");
  auto expr_span = absl::MakeConstSpan(exprs);
  auto result = ParsePublicItem(StringViewTokenNode("public"), expr_span);
  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value().as<PublicDef>().entries(), testing::SizeIs(2));
}

}  // namespace

}  // namespace parsers::sci