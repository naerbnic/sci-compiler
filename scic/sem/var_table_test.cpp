#include "scic/sem/var_table.hpp"

#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "scic/sem/test_helpers.hpp"
#include "util/status/status_matchers.hpp"

namespace sem {
namespace {

TEST(VarTableTest, SingleVariableDeclaration) {
  auto builder = GlobalTableBuilder::Create();
  // Declare a single variable.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("var1"), 0));

  // Build the table.
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  // Lookup by name.
  auto var_ptr = table->LookupByName("var1");
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(var_ptr->index(), 0);

  // Lookup by index.
  var_ptr = table->LookupByIndex(0);
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(std::string_view(var_ptr->name()), "var1");
}

TEST(VarTableTest, DuplicateDeclarationNoOp) {
  auto builder = GlobalTableBuilder::Create();
  // Declare the same variable twice (same name and index).
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("dupVar"), 42));
  // Duplicate invocation should be a no-op.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("dupVar"), 42));

  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());
  auto var_ptr = table->LookupByName("dupVar");
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(var_ptr->index(), 42);
}

TEST(VarTableTest, ConflictingNameDeclarationError) {
  auto builder = GlobalTableBuilder::Create();
  // Declare a variable.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("conflict"), 1));
  // Re-declaring the same name with a different index should error.
  auto status = builder->DeclareVar(CreateTestNameToken("conflict"), 2);
  EXPECT_FALSE(status.ok());
}

TEST(VarTableTest, ConflictingIndexDeclarationError) {
  auto builder = GlobalTableBuilder::Create();
  // Declare a variable.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("varA"), 100));
  // Declaring a different variable with the same index should error.
  auto status = builder->DeclareVar(CreateTestNameToken("varB"), 100);
  EXPECT_FALSE(status.ok());
}

TEST(VarTableTest, LookupByIndexAndName) {
  auto builder = GlobalTableBuilder::Create();
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("alpha"), 10));
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("beta"), 20));

  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  // Lookup by name.
  auto varAlpha = table->LookupByName("alpha");
  auto varBeta = table->LookupByName("beta");
  ASSERT_NE(varAlpha, nullptr);
  ASSERT_NE(varBeta, nullptr);
  EXPECT_EQ(varAlpha->index(), 10);
  EXPECT_EQ(varBeta->index(), 20);

  // Lookup by index.
  varAlpha = table->LookupByIndex(10);
  varBeta = table->LookupByIndex(20);
  ASSERT_NE(varAlpha, nullptr);
  ASSERT_NE(varBeta, nullptr);
  EXPECT_EQ(std::string_view(varAlpha->name()), "alpha");
  EXPECT_EQ(std::string_view(varBeta->name()), "beta");
}

}  // namespace
}  // namespace sem