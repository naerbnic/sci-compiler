#include "scic/tokenizer/char_stream.hpp"

#include <gtest/gtest.h>

namespace tokenizer {
namespace {

TEST(TextContentsTest, CalculatesCorrectNumberOfLines) {
  TextContents contents(
      "abc\n"
      "def\r"
      "ghi\r\n"
      "jkl");
  EXPECT_EQ(contents.num_lines(), 4);
}

TEST(TextContentsTest, TrailingNewlineCountsAsALine) {
  TextContents contents("foobar\n");
  EXPECT_EQ(contents.num_lines(), 2);
}

TEST(TextContentsTest, GetLinesWorks) {
  TextContents contents(
      "abc\n"
      "def\r"
      "ghi\r\n"
      "jkl");
  EXPECT_EQ(contents.GetLine(0), "abc");
  EXPECT_EQ(contents.GetLine(1), "def");
  EXPECT_EQ(contents.GetLine(2), "ghi");
  EXPECT_EQ(contents.GetLine(3), "jkl");
}

}  // namespace
}  // namespace tokenizer
