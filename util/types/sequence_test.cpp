#include "util/types/sequence.hpp"

#include <array>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace util {
namespace {

using ::testing::ElementsAre;

static_assert(std::is_default_constructible_v<Seq<int>>);
static_assert(std::is_copy_constructible_v<Seq<int>>);
static_assert(std::is_move_constructible_v<Seq<int>>);
static_assert(std::is_copy_assignable_v<Seq<int>>);
static_assert(std::is_move_assignable_v<Seq<int>>);

static_assert(std::ranges::random_access_range<Seq<int>>);

TEST(SequenceTest, Empty) {
  std::array<int, 0> arr;
  Seq<int> seq = arr;
  EXPECT_EQ(seq.size(), 0);
}

TEST(SequenceTest, SimpleSequence) {
  std::array<int, 3> arr = {1, 2, 3};
  Seq<int> seq = arr;
  EXPECT_EQ(seq.size(), 3);
  EXPECT_THAT(seq, ElementsAre(1, 2, 3));
}

TEST(SequenceTest, SimpleConstSequence) {
  const std::array<int, 3> arr = {1, 2, 3};
  Seq<int> seq = arr;
  EXPECT_EQ(seq.size(), 3);
  EXPECT_THAT(seq, ElementsAre(1, 2, 3));
}

TEST(SequenceTest, SimpleRefSequence) {
  std::array<int, 3> arr = {1, 2, 3};
  Seq<int&> seq = arr;
  EXPECT_THAT(seq, ElementsAre(1, 2, 3));
  seq[0] = 4;
  EXPECT_THAT(arr, ElementsAre(4, 2, 3));
}

TEST(SequenceTest, SimpleTransform) {
  std::array<int, 3> arr = {1, 2, 3};
  auto seq = Seq<int>::CreateTransform(arr, [](int x) { return x * 2; });
  EXPECT_EQ(seq.size(), 3);
  EXPECT_THAT(seq, ElementsAre(2, 4, 6));
}

TEST(SequenceTest, Singleton) {
  int x = 42;
  auto seq = Seq<int>::Singleton(x);
  EXPECT_EQ(seq.size(), 1);
  EXPECT_EQ(seq[0], 42);
  EXPECT_THAT(seq, ElementsAre(42));
}

}  // namespace
}  // namespace util
