#include "scic/parsers/combinators/combinators.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "absl/types/span.h"

namespace parsers {
namespace {

static_assert(Streamable<ElemStream<int>>);
static_assert(Streamable<SpanStream<int>>);

TEST(SpanStreamTest, BasicWorks) {
  absl::Span<char const> span(std::string_view("abc"));
  auto stream = SpanStream(span);
  EXPECT_EQ(*stream, 'a');
  EXPECT_EQ(*stream++, 'a');
  EXPECT_EQ(*stream, 'b');
  EXPECT_EQ(*++stream, 'c');
  EXPECT_TRUE(bool(stream));
  EXPECT_EQ(*stream++, 'c');
  EXPECT_FALSE(bool(stream));
}

TEST(ElemStreamTest, BasicWorks) {
  absl::Span<char const> span(std::string_view("abc"));
  auto stream = ElemStream(SpanStream(span));
  EXPECT_EQ(*stream, 'a');
  EXPECT_EQ(*stream++, 'a');
  EXPECT_EQ(*stream, 'b');
  EXPECT_EQ(*++stream, 'c');
  EXPECT_TRUE(bool(stream));
  EXPECT_EQ(*stream++, 'c');
  EXPECT_FALSE(bool(stream));
}

TEST(ParseResultTest, SimpleWorks) {
  auto result = ParseResult(0UL, std::string("Hello"));
}

}  // namespace
}  // namespace parsers