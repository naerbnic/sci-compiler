#include "util/types/choice_matchers.hpp"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/types/choice.hpp"

namespace util {
namespace {

using testing::_;
using testing::Not;

class SimpleValue : public ChoiceBase<SimpleValue, int, std::string> {
  using ChoiceBase::ChoiceBase;
};

TEST(ChoiceTest, Basic) {
  SimpleValue choice = SimpleValue(42);
  EXPECT_THAT(choice, ChoiceOf<int>(_));
  EXPECT_THAT(choice, ChoiceOf<int>(42));
  EXPECT_THAT(choice, Not(ChoiceOf<int>(12)));
  EXPECT_THAT(choice, Not(ChoiceOf<std::string>(_)));
}
}  // namespace
}  // namespace util