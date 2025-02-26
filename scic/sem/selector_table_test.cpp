#include "scic/sem/selector_table.hpp"

#include "gtest/gtest.h"
#include "scic/sem/common.hpp"
#include "scic/text/text_range.hpp"
#include "util/status/status_matchers.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {
namespace {

TEST(SelectorTableTest, BasicTest) {
  auto builder = SelectorTable::CreateBuilder();
  ASSERT_OK(builder->DeclareSelector(
      NameToken(util::RefStr("hello"), text::TextRange::OfString("hello")),
      SelectorNum::Create(0)));
  ASSERT_OK(builder->AddNewSelector(NameToken(
      util::RefStr("goodbye"), text::TextRange::OfString("goodbye"))));
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  EXPECT_EQ(table->LookupByName("hello")->selector_num().value(), 0);
  EXPECT_EQ(table->LookupByName("goodbye")->selector_num().value(), 1);
}

TEST(SelectorTableTest, RepeatedDeclIsOkay) {
  auto builder = SelectorTable::CreateBuilder();
  ASSERT_OK(builder->DeclareSelector(
      NameToken(util::RefStr("-objID-"), text::TextRange::OfString("-objID-")),
      SelectorNum::Create(4096)));
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());
}
}  // namespace
}  // namespace sem
