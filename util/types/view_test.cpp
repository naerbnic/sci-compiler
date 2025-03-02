#include "util/types/view.hpp"

#include <gtest/gtest.h>

#include <string>

namespace util {
namespace {

TEST(ViewTest, Basic) {
  std::string str = "hello";
  View<std::string const> view = str;
  EXPECT_EQ(view.value(), "hello");
}

TEST(ViewTest, EqualityWorks) {
  std::string str = "hello";
  View<std::string const> view1 = str;
  View<std::string const> view2 = str;
  EXPECT_EQ(view1, view2);
}

TEST(ViewTest, ComparisonWorks) {
  std::string str1 = "hello";
  std::string str2 = "world";
  View<std::string const> view1 = str1;
  View<std::string const> view2 = str2;
  EXPECT_LT(view1, view2);
}

}  // namespace
}  // namespace util