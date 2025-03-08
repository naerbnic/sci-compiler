#include "util/types/name.hpp"

#include "gtest/gtest.h"

class Foo {};

namespace util {

class Bar {};
namespace {

TEST(NameTest, BasicWorks) { EXPECT_EQ(TypeName<Foo>(), "Foo"); }
TEST(NameTest, InNamespaceWorks) {
  EXPECT_EQ(TypeName<Bar>(), "util::Bar");
}

}  // namespace
}  // namespace util