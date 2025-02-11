#include "scic/parsers/sci/ast.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "scic/text/text_range.hpp"

namespace parsers::sci {

TEST(TokenNodeTest, ValueCase) {
  auto text_range = text::TextRange::OfString("foo");
  auto node = TokenNode<std::string>("foo", text_range);
  EXPECT_EQ(*node, "foo");
  EXPECT_EQ(node->size(), 3);
  EXPECT_EQ(*node.get(), "foo");
}

TEST(TokenNodeTest, RawPointerCase) {
  std::string value = "foo";
  auto text_range = text::TextRange::OfString("foo");
  auto node = TokenNode<std::string*>(&value, text_range);
  EXPECT_EQ(*node, "foo");
  EXPECT_EQ(node->size(), 3);
  EXPECT_EQ(*node.get(), "foo");
}

TEST(TokenNodeTest, SmartPtrCase) {
  const std::string value = "foo";
  auto text_range = text::TextRange::OfString("foo");
  auto node = TokenNode<std::unique_ptr<std::string>>(
      std::make_unique<std::string>("foo"), text_range);
  EXPECT_EQ(*node, "foo");
  EXPECT_EQ(node->size(), 3);
  EXPECT_EQ(*node.get(), "foo");
}

}  // namespace parsers::sci