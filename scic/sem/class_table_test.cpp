#include "scic/sem/class_table.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_matchers.hpp"

namespace sem {
namespace {

using ::testing::ElementsAre;

TEST(ClassTableTest, EmptyTest) {
  ASSERT_OK_AND_ASSIGN(auto sel_table, SelectorTable::CreateBuilder()->Build());
  auto builder = ClassTableBuilder::Create(sel_table.get());
  ASSERT_OK_AND_ASSIGN(auto class_table, builder->Build());
  EXPECT_THAT(class_table->decl_classes(), ElementsAre());
  EXPECT_THAT(class_table->classes(), ElementsAre());
}

}  // namespace
}  // namespace sem
