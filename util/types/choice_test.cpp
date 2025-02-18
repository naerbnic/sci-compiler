#include "util/types/choice.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace util {
namespace {

class SimpleValue : public ChoiceBase<SimpleValue, int, std::string> {
  using ChoiceBase::ChoiceBase;
};

TEST(ChoiceTest, Basic) {
  SimpleValue choice = SimpleValue(42);
  EXPECT_TRUE(choice.has<int>());
  EXPECT_EQ(choice.as<int>(), 42);
  EXPECT_FALSE(choice.has<std::string>());
  EXPECT_EQ(*choice.try_get<int>(), 42);
  EXPECT_EQ(choice.try_get<std::string>(), nullptr);

  choice = SimpleValue::Make<std::string>("foo");
  EXPECT_FALSE(choice.has<int>());
  EXPECT_TRUE(choice.has<std::string>());
  EXPECT_EQ(choice.as<std::string>(), "foo");
  EXPECT_EQ(choice.try_get<int>(), nullptr);
  EXPECT_EQ(*choice.try_get<std::string>(), "foo");
}

class MoveOnlyValue : public ChoiceBase<MoveOnlyValue, std::unique_ptr<int>,
                                        std::unique_ptr<std::string>> {
  using ChoiceBase::ChoiceBase;
};

TEST(ChoiceTest, MoveOnly) {
  MoveOnlyValue choice = MoveOnlyValue(std::unique_ptr<int>(new int(42)));
  EXPECT_TRUE(choice.has<std::unique_ptr<int>>());
  EXPECT_EQ(*choice.as<std::unique_ptr<int>>(), 42);
  EXPECT_FALSE(choice.has<std::unique_ptr<std::string>>());
  EXPECT_EQ(**choice.try_get<std::unique_ptr<int>>(), 42);
  EXPECT_EQ(choice.try_get<std::unique_ptr<std::string>>(), nullptr);

  choice = MoveOnlyValue::Make<std::unique_ptr<std::string>>(
      std::make_unique<std::string>("foo"));
  EXPECT_FALSE(choice.has<std::unique_ptr<int>>());
  EXPECT_TRUE(choice.has<std::unique_ptr<std::string>>());
  EXPECT_EQ(*choice.as<std::unique_ptr<std::string>>(), "foo");
  EXPECT_EQ(choice.try_get<std::unique_ptr<int>>(), nullptr);
  EXPECT_EQ(**choice.try_get<std::unique_ptr<std::string>>(), "foo");
}
}  // namespace
}  // namespace util