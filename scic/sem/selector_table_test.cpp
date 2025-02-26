#include "scic/sem/selector_table.hpp"

#include "gtest/gtest.h"
#include "scic/sem/common.hpp"
#include "scic/sem/test_helpers.hpp"
#include "util/status/status_matchers.hpp"

namespace sem {
namespace {

TEST(SelectorTableTest, BasicTest) {
  auto builder = SelectorTable::CreateBuilder();
  ASSERT_OK(builder->DeclareSelector(CreateTestNameToken("hello"),
                                     SelectorNum::Create(0)));
  ASSERT_OK(builder->AddNewSelector(CreateTestNameToken("goodbye")));
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  EXPECT_EQ(table->LookupByName("hello")->selector_num().value(), 0);
  EXPECT_EQ(table->LookupByName("goodbye")->selector_num().value(), 1);
  EXPECT_EQ(table->LookupByNumber(SelectorNum::Create(0))->name(), "hello");
  EXPECT_EQ(table->LookupByNumber(SelectorNum::Create(1))->name(), "goodbye");
}

TEST(SelectorTableTest, RepeatedDeclIsOkay) {
  auto builder = SelectorTable::CreateBuilder();
  ASSERT_OK(builder->DeclareSelector(CreateTestNameToken("-objID-"),
                                     SelectorNum::Create(4096)));
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());
}
}  // namespace
}  // namespace sem
