#include "util/types/tmpl.hpp"

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <vector>

namespace util {
namespace {

TEST(TmplTest, IsSpecialization) {
  static_assert(
      TemplateTraits<std::vector>::IsSpecialization<std::vector<int>>);
  static_assert(!TemplateTraits<std::vector>::IsSpecialization<std::string>);
}

TEST(TmplTest, Type) {
  static_assert(std::is_same_v<TemplateTraits<std::vector>::Apply<int>,
                               std::vector<int>>);
}

TEST(TmplTest, SpecializationArgs) {
  static_assert(
      TemplateTraits<std::vector>::SpecializationArgs<std::vector<int>>::Size ==
      2);
}

}  // namespace
}  // namespace util