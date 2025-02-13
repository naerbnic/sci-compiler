#include "util/types/name.hpp"

#include "gtest/gtest.h"

class Foo {};

namespace util {
namespace {

class Bar {};

TEST(NameTest, BasicWorks) { EXPECT_EQ(TypeName<Foo>(), "Foo"); }
TEST(NameTest, InNamespaceWorks) {
  EXPECT_EQ(TypeName<Bar>(), "util::(anonymous namespace)::Bar");
}

}  // namespace
}  // namespace util