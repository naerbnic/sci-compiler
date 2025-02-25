#include "util/types/opt.hpp"

#include <optional>
#include <utility>

#include "gtest/gtest.h"

namespace util {
namespace {

TEST(OptTest, BasicWorks) {
  Opt<int> opt;
  EXPECT_FALSE(opt);
  EXPECT_FALSE(opt.has_value());
  EXPECT_EQ(opt.value_or(5), 5);
  opt.emplace(3);
  EXPECT_TRUE(opt);
  EXPECT_TRUE(opt.has_value());
  EXPECT_EQ(opt.value(), 3);
  EXPECT_EQ(opt.value_or(5), 3);
  opt.reset();
  EXPECT_FALSE(opt);
  EXPECT_FALSE(opt.has_value());
  EXPECT_EQ(opt.value_or(5), 5);
}

TEST(OptTest, RefWorks) {
  int value = 3;
  Opt<int&> opt(value);
  EXPECT_TRUE(opt);
  EXPECT_TRUE(opt.has_value());
  EXPECT_EQ(opt.value(), 3);
  EXPECT_EQ(opt.value_or(5), 3);
  opt.reset();
  EXPECT_FALSE(opt);
  EXPECT_FALSE(opt.has_value());
  EXPECT_EQ(opt.value_or(5), 5);

  Opt<int&> opt2(std::in_place, value);

  EXPECT_TRUE(opt != opt2);
  EXPECT_TRUE(opt == std::nullopt);
  EXPECT_TRUE(std::nullopt == opt);
  opt = Opt<int&>(value);
  EXPECT_TRUE(opt == opt2);
}

TEST(OptTest, RefToBasicWorks) {
  Opt<int> value_opt = 3;
  Opt<int const&> ref_opt = value_opt;
  EXPECT_EQ(ref_opt.value(), 3);
  value_opt.value() = 4;
  EXPECT_EQ(ref_opt.value(), 4);
}

}  // namespace
}  // namespace util