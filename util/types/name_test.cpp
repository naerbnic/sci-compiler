#include "util/types/name.hpp"

#include "gtest/gtest.h"

class Foo {};

namespace util {

class Bar {};
namespace {

#if defined(__clang__) || defined(__GNUC__)
TEST(NameTest, BasicWorks) { EXPECT_EQ(TypeName<Foo>(), "Foo"); }
TEST(NameTest, InNamespaceWorks) { EXPECT_EQ(TypeName<Bar>(), "util::Bar"); }
#else
// In Windows for now, disable the test as I need to investigate the issue.
TEST(NameTest, BasicWorks) { EXPECT_EQ(TypeName<Foo>(), "class Foo"); }
TEST(NameTest, InNamespaceWorks) { EXPECT_EQ(TypeName<Bar>(), "class util::Bar"); }
#endif

}  // namespace
}  // namespace util