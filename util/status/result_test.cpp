#include "util/status/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "gtest/gtest.h"

namespace util {
namespace {

TEST(ResultTest, ValueOf) {
  Result<int, std::string> result = Result<int, std::string>::ValueOf(42);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorOf) {
  Result<int, std::string> result = Result<int, std::string>::ErrorOf("error");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), "error");
}

TEST(ResultTest, ImplicitConversion) {
  Result<int, std::string> result = 42;
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ImplicitConversionError) {
  Result<int, std::string> result = "error";
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), "error");
}

TEST(ResultTest, PointerSemantics) {
  Result<std::string, int> result = std::string("success");
  EXPECT_EQ(*result, "success");
  EXPECT_EQ(result->c_str(), std::string_view("success"));
}

TEST(ResultTest, AssignmentOperator) {
  Result<int, std::string> result;
  result = 42;
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 42);

  result = "error";
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), "error");
}

TEST(ResultTest, ConversionFromAnotherResult) {
  Result<std::uint8_t, std::string> result1 = std::uint8_t(42);
  Result<std::uint16_t, std::string_view> result2 = std::move(result1);
  EXPECT_TRUE(result2.ok());
  EXPECT_EQ(result2.value(), 42);
}

TEST(ApplyResultTest, AllSuccessesWorks) {
  auto result = ApplyResults([](int a, int b) { return a + b; },
                             Result<int, std::string>::ValueOf(1),
                             Result<int, std::string>::ValueOf(2));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 3);
}

TEST(ApplyResultTest, GetsFirstErrorIfNotMergeable) {
  using ResultType = Result<int, std::string>;
  auto result = ApplyResults(
      [](int a, int b, int c) { return a + b; }, ResultType::ValueOf(1),
      ResultType::ErrorOf("error1"), ResultType::ErrorOf("error2"));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), "error1");
}

}  // namespace
}  // namespace util