#include "scic/sem/var_table.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/test_helpers.hpp"
#include "util/status/status_matchers.hpp"

// Assuming you have some helper to create a dummy LiteralValue.
static std::vector<codegen::LiteralValue> CreateTestInitialValue(
    std::size_t count, int value) {
  return std::vector<codegen::LiteralValue>(count,
                                            codegen::LiteralValue(value));
}

namespace sem {
namespace {

//
// VarTable (fully defined variables) tests
//
TEST(VarTableTest, SingleVariableFullDefinition) {
  auto builder = VarTableBuilder::Create();
  // Then define the variable with an initial value.
  ASSERT_OK(builder->DefineVar(CreateTestNameToken("var1"),
                               ModuleVarIndex::Create(0),
                               CreateTestInitialValue(1, 123)));

  // Build the table.
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  // Lookup by name.
  auto var_ptr = table->LookupByName("var1");
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(var_ptr->index().value(), 0);
  EXPECT_EQ(var_ptr->initial_value().size(), 1u);

  // Lookup by index.
  var_ptr = table->LookupByIndex(ModuleVarIndex::Create(0));
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(std::string_view(var_ptr->name()), "var1");
}

TEST(VarTableTest, DuplicateDeclarationNoOpFullDefinition) {
  auto builder = VarTableBuilder::Create();
  ASSERT_OK(builder->DefineVar(CreateTestNameToken("dupVar"),
                               ModuleVarIndex::Create(42),
                               CreateTestInitialValue(1, 123)));

  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());
  auto var_ptr = table->LookupByName("dupVar");
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(var_ptr->index().value(), 42);
}

TEST(VarTableTest, LookupByIndexAndNameFullDefinition) {
  auto builder = VarTableBuilder::Create();
  // Define the variables.
  ASSERT_OK(builder->DefineVar(CreateTestNameToken("alpha"),
                               ModuleVarIndex::Create(10),
                               CreateTestInitialValue(1, 123)));
  ASSERT_OK(builder->DefineVar(CreateTestNameToken("beta"),
                               ModuleVarIndex::Create(20),
                               CreateTestInitialValue(1, 456)));

  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  // Lookup by name.
  auto varAlpha = table->LookupByName("alpha");
  auto varBeta = table->LookupByName("beta");
  ASSERT_NE(varAlpha, nullptr);
  ASSERT_NE(varBeta, nullptr);
  EXPECT_EQ(varAlpha->index().value(), 10);
  EXPECT_EQ(varBeta->index().value(), 20);

  // Lookup by index.
  varAlpha = table->LookupByIndex(ModuleVarIndex::Create(10));
  varBeta = table->LookupByIndex(ModuleVarIndex::Create(20));
  ASSERT_NE(varAlpha, nullptr);
  ASSERT_NE(varBeta, nullptr);
  EXPECT_EQ(std::string_view(varAlpha->name()), "alpha");
  EXPECT_EQ(std::string_view(varBeta->name()), "beta");
}

//
// VarDeclTable tests
//
TEST(VarDeclTableTest, SingleVariableDeclaration) {
  auto builder = VarDeclTableBuilder::Create();
  // Declare a single variable.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("var_decl1"),
                                GlobalIndex::Create(0), 1));

  // Build the declaration table.
  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  // Lookup by name.
  auto var_ptr = table->LookupByName("var_decl1");
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(var_ptr->index().value(), 0);

  // Lookup by index.
  var_ptr = table->LookupByIndex(GlobalIndex::Create(0));
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(std::string_view(var_ptr->name()), "var_decl1");
}

TEST(VarDeclTableTest, DuplicateDeclarationNoOpDeclOnly) {
  auto builder = VarDeclTableBuilder::Create();
  // Declare the same variable twice (same name and index) is a no-op.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("dupDecl"),
                                GlobalIndex::Create(42), 1));
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("dupDecl"),
                                GlobalIndex::Create(42), 1));

  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());
  auto var_ptr = table->LookupByName("dupDecl");
  ASSERT_NE(var_ptr, nullptr);
  EXPECT_EQ(var_ptr->index().value(), 42);
}

TEST(VarDeclTableTest, ConflictingNameDeclarationErrorDeclOnly) {
  auto builder = VarDeclTableBuilder::Create();
  // Declare a variable.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("conflict_decl"),
                                GlobalIndex::Create(1), 1));
  // Re-declaring the same name with a different index should error.
  auto status = builder->DeclareVar(CreateTestNameToken("conflict_decl"),
                                    GlobalIndex::Create(2), 1);
  EXPECT_FALSE(status.ok());
}

TEST(VarDeclTableTest, ConflictingIndexDeclarationErrorDeclOnly) {
  auto builder = VarDeclTableBuilder::Create();
  // Declare a variable.
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("varA_decl"),
                                GlobalIndex::Create(100), 1));
  // Declaring a different variable with the same index should error.
  auto status = builder->DeclareVar(CreateTestNameToken("varB_decl"),
                                    GlobalIndex::Create(100), 1);
  EXPECT_FALSE(status.ok());
}

TEST(VarDeclTableTest, LookupByIndexAndNameDeclOnly) {
  auto builder = VarDeclTableBuilder::Create();
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("alpha_decl"),
                                GlobalIndex::Create(10), 1));
  ASSERT_OK(builder->DeclareVar(CreateTestNameToken("beta_decl"),
                                GlobalIndex::Create(20), 1));

  ASSERT_OK_AND_ASSIGN(auto table, builder->Build());

  // Lookup by name.
  auto varAlpha = table->LookupByName("alpha_decl");
  auto varBeta = table->LookupByName("beta_decl");
  ASSERT_NE(varAlpha, nullptr);
  ASSERT_NE(varBeta, nullptr);
  EXPECT_EQ(varAlpha->index().value(), 10);
  EXPECT_EQ(varBeta->index().value(), 20);

  // Lookup by index.
  varAlpha = table->LookupByIndex(GlobalIndex::Create(10));
  varBeta = table->LookupByIndex(GlobalIndex::Create(20));
  ASSERT_NE(varAlpha, nullptr);
  ASSERT_NE(varBeta, nullptr);
  EXPECT_EQ(std::string_view(varAlpha->name()), "alpha_decl");
  EXPECT_EQ(std::string_view(varBeta->name()), "beta_decl");
}

}  // namespace
}  // namespace sem