#include "util/types/strong_types.hpp"

#include <memory>

#include "gtest/gtest.h"

namespace util {
namespace {

struct SimpleRefTag : ReferenceTag<std::unique_ptr<int>> {};

using UniqueInt = StrongValue<SimpleRefTag>;
using UniqueIntView = StrongView<SimpleRefTag>;

TEST(StrongTypesTest, MoveValue) {
  auto v1 = UniqueInt::Create(std::make_unique<int>(42));
  EXPECT_EQ(*v1.value(), 42);

  UniqueInt::View view = v1;
  EXPECT_EQ(*view.value(), 42);

  *v1.value() = 21;
  EXPECT_EQ(*view.value(), 21);
}

struct ValueOnlyTag {
  using Value = int;
  static constexpr bool is_const = false;
};

using ValueOnlyType = StrongValue<ValueOnlyTag>;

TEST(StrongTypesTest, ValueOnlyWorks) {
  ValueOnlyType v1 = ValueOnlyType::Create(42);
  EXPECT_EQ(v1.value(), 42);

  auto v2 = v1;

  v1.value() = 21;

  EXPECT_EQ(v1.value(), 21);
  EXPECT_EQ(v2.value(), 42);
}

}  // namespace
}  // namespace util