#include "util/strings/ref_str.hpp"

#include <sstream>
#include <string_view>
#include <unordered_map>

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"

namespace util {
namespace {

TEST(RefStrTest, EmptyString) {
  RefStr str;
  EXPECT_EQ(std::string_view(str), "");
}

TEST(RefStrTest, SimpleTest) {
  RefStr foo = "hello";
  EXPECT_EQ(foo, "hello");
  EXPECT_EQ("hello", foo);
}

TEST(RefStrTest, BasicStringOperationsWork) {
  RefStr foo = "hello";
  auto result = absl::StrFormat("%s world", foo);
  EXPECT_EQ(result, "hello world");

  std::stringstream ss;
  ss << foo;
  EXPECT_EQ(ss.str(), "hello");
}

TEST(RefStrTest, InStdMapWorks) {
  std::unordered_map<RefStr, int> map;

  map.emplace("foo", 1);
  map.emplace("bar", 2);

  EXPECT_EQ(map["foo"], 1);
  EXPECT_EQ(map["bar"], 2);
}

}  // namespace
}  // namespace util