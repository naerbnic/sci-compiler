#include "scic/parsers/sci/parser.hpp"

#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/parser_test_utils.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "util/choice_matchers.hpp"

namespace parsers::sci {
namespace {

using ::testing::_;
using ::testing::ElementsAre;

ParseResult<std::vector<Item>> TryParseItems(std::string_view text) {
  auto exprs = list_tree::ParseExprsOrDie(text);
  return ParseItems(exprs);
}

TEST(ParseItemsTest, Empty) {
  auto result = TryParseItems("");
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());
}

TEST(ParseItemsTest, Public) {
  auto result = TryParseItems("(public foo 1 bar 2)");
  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<PublicDef>(_)));
}

}  // namespace
}  // namespace parsers::sci
