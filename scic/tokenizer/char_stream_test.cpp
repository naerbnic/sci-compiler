#include "scic/tokenizer/char_stream.hpp"

#include <gtest/gtest.h>

#include "scic/tokenizer/text_contents.hpp"

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

TEST(TextContentsTest, GetLineIndexWorks) {
  TextContents contents(
      "abc\n"
      "def\r"
      "ghi\r\n"
      "jkl");
  // Each line should be 4 characters, including the one with \r\n, as there
  // is no newline at the end of the file.
  auto offset = contents.GetOffset(3);
  EXPECT_EQ(offset.line_index(), 0);
  EXPECT_EQ(offset.column_index(), 3);

  offset = contents.GetOffset(4);
  EXPECT_EQ(offset.line_index(), 1);
  EXPECT_EQ(offset.column_index(), 0);
}

TEST(CharStreamTest, PreIncrementWorks) {
  CharStream stream("abc");
  EXPECT_EQ(*++stream, 'b');
  EXPECT_EQ(*++stream, 'c');
  EXPECT_TRUE(stream);
}

TEST(CharStreamTest, PostIncrementWorks) {
  CharStream stream("abc");
  EXPECT_EQ(*stream++, 'a');
  EXPECT_EQ(*stream++, 'b');
  EXPECT_EQ(*stream++, 'c');
  EXPECT_FALSE(stream);
}

TEST(CharStreamTest, ReadsWindowsNewlinesAsSingleNewlines) {
  CharStream stream("a\r\nb");
  EXPECT_EQ(*stream++, 'a');
  EXPECT_EQ(*stream++, '\n');
  EXPECT_EQ(*stream++, 'b');
}

TEST(CharStreamTest, StreamIsCopyable) {
  CharStream stream1("abc");
  EXPECT_EQ(*stream1++, 'a');
  CharStream stream2 = stream1;
  EXPECT_EQ(*stream1++, 'b');
  EXPECT_EQ(*stream1++, 'c');

  EXPECT_EQ(*stream2++, 'b');
  EXPECT_EQ(*stream2++, 'c');
}

}  // namespace
}  // namespace tokenizer
