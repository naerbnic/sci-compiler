#include "scic/parsers/combinators/combinators.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <utility>

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

TEST(ParseResultTest, SimpleApplyWorks) {
  ParseResult<int> result(5);
  auto new_result = result.Apply([](int value) { return value + 1; });
  EXPECT_EQ(new_result.ok(), true);
  EXPECT_EQ(new_result.value(), 6);
}

TEST(ParseResultTest, MoveOnlyApplyWorks) {
  ParseResult<std::unique_ptr<int>> result(std::make_unique<int>(5));
  auto new_result = std::move(result).Apply([](std::unique_ptr<int> value) {
    return std::make_unique<int>(*value + 1);
  });
  EXPECT_EQ(new_result.ok(), true);
  EXPECT_EQ(*new_result.value(), 6);
}

TEST(ParseResultTest, BinaryApplyWorks) {
  ParseResult<int> result1(5);
  ParseResult<int> result2(6);
  auto new_result =
      (result1 | result2).Apply([](int a, int b) { return a + b; });
  EXPECT_EQ(new_result.ok(), true);
  EXPECT_EQ(new_result.value(), 11);
}

TEST(ParseResultTest, BinaryMoveApplyWorks) {
  ParseResult<std::unique_ptr<int>> result1(std::make_unique<int>(5));
  ParseResult<std::unique_ptr<int>> result2(std::make_unique<int>(6));
  auto new_result =
      (std::move(result1) | std::move(result2))
          .Apply([](std::unique_ptr<int> a, std::unique_ptr<int> b) {
            return std::make_unique<int>(*a + *b);
          });
  EXPECT_EQ(new_result.ok(), true);
  EXPECT_EQ(*new_result.value(), 11);
}

}  // namespace
}  // namespace parsers