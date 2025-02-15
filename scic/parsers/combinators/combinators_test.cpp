#include "scic/parsers/combinators/combinators.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <utility>

#include "absl/types/span.h"
#include "scic/parsers/combinators/results.hpp"

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

TEST(ParseResultTest, SimpleValueWorks) {
  ParseResult<int> result(5);
  EXPECT_EQ(result.ok(), true);
  EXPECT_EQ(result.value(), 5);
}

TEST(ParseResultTest, MoveOnlyWorks) {
  ParseResult<std::unique_ptr<int>> result(std::make_unique<int>(5));
  EXPECT_EQ(result.ok(), true);
  EXPECT_EQ(*result.value(), 5);

  std::unique_ptr<int> take_value = std::move(result).value();
  EXPECT_EQ(*take_value, 5);
  EXPECT_EQ(result.value(), nullptr);
}

}  // namespace
}  // namespace parsers