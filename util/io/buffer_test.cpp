#include "util/io/buffer.hpp"

#include <gtest/gtest.h>

namespace io {
namespace {

TEST(ReadBufferTest, FindNewline) {
  auto str_buffer = ReadBuffer::FromString(
      "abc\n"
      "def\r"
      "ghi\r\n"
      "jkl");

  // First line is loaded.
  EXPECT_EQ(str_buffer->CurrLine(), "abc\n");

  // Curr line does not advance.
  EXPECT_EQ(str_buffer->CurrLine(), "abc\n");

  str_buffer->AdvanceLine();
  EXPECT_EQ(str_buffer->CurrLine(), "def\r");

  str_buffer->AdvanceLine();
  EXPECT_EQ(str_buffer->CurrLine(), "ghi\r\n");

  str_buffer->AdvanceLine();
  EXPECT_EQ(str_buffer->CurrLine(), "jkl");
  EXPECT_FALSE(str_buffer->IsAtEnd());
  str_buffer->AdvanceLine();
  EXPECT_EQ(str_buffer->CurrLine(), "");
  EXPECT_TRUE(str_buffer->IsAtEnd());
}
}  // namespace
}  // namespace io
