#include "util/types/any_storage.hpp"

#include "gtest/gtest.h"

namespace util {
namespace {

TEST(AnyStorageTest, BasicWorks) {
  AnyStorage<int> storage1;
  storage1 = AnyStorage<int>(5);
  EXPECT_EQ(storage1.value(), 5);

  AnyStorage<int> storage2 = storage1;
  EXPECT_EQ(storage2.value(), 5);
}

TEST(AnyStorageTest, BasicRefWorks) {
  int value = 5;
  AnyStorage<int&> storage1(value);
  EXPECT_EQ(storage1.value(), 5);

  storage1.value() = 6;
  EXPECT_EQ(value, 6);

  AnyStorage<int&> storage2 = storage1;
  EXPECT_EQ(storage2.value(), 6);

  storage2.value() = 10;
  EXPECT_EQ(value, 10);
  EXPECT_EQ(storage1.value(), 10);

  // AnyStorage assignment always assigns by value.
  int value2 = 20;
  storage2 = AnyStorage<int&>(value2);
  EXPECT_EQ(storage2.value(), 20);
  EXPECT_EQ(value, 10);
  EXPECT_EQ(storage1.value(), 10);
}

}  // namespace
}  // namespace util