#include "scic/parsers/sci/parser.hpp"

#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/parser_test_utils.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "util/types/choice_matchers.hpp"

namespace parsers::sci {
namespace {

using ::testing::_;
using ::testing::ElementsAre;

ParseResult<std::vector<Item>> TryParseItems(std::string_view text) {
  auto exprs = list_tree::ParseExprsOrDie(text);
  return ParseItems(exprs);
}

TEST(ParseItemsTest, Empty) {
  auto result = TryParseItems("");
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());
}

TEST(ParseItemsTest, ScriptNum) {
  auto result = TryParseItems(R"(
        (script# 111)
    )");

  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ScriptNumDef>(_)));
}

TEST(ParseItemsTest, Public) {
  auto result = TryParseItems("(public foo 1 bar 2)");
  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<PublicDef>(_)));
}

TEST(ParseItemsTest, Extern) {
  auto result = TryParseItems(R"(
        (extern foo -1 0)
    )");

  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ExternDef>(_)));
}

TEST(ParseItemsTest, GlobalDecl) {
  auto result = TryParseItems(R"(
        (globaldecl foo 0)
    )");

  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<GlobalDeclDef>(_)));
}

TEST(ParseItemsTest, Global) {
  auto result = TryParseItems(R"(
        (global
            [foo 4] 0 = [1 2 "Hello"])
    )");

  ASSERT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ModuleVarsDef>(_)));
}

TEST(ParseItemsTest, Local) {
  auto result = TryParseItems(R"(
        (local
            [foo 4] 0 = [1 2 "Hello"])
    )");

  ASSERT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ModuleVarsDef>(_)));
}

TEST(ParseItemsTest, Proc) {
  auto result = TryParseItems(R"(
        (procedure (foo) (= a 1) (return))
    )");

  EXPECT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ProcDef>(_)));
}

TEST(ParseItemsTest, Class) {
  auto result = TryParseItems(R"(
          (class Foo of Bar
              (properties baz 1)
              (methods quux)
              (method (mu a b &temp c)
                (return)))
      )");

  EXPECT_TRUE(result.ok()) << result.status().messages()[0].primary().message();
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ClassDef>(_)));
}

TEST(ParseItemsTest, Instance) {
  auto result = TryParseItems(R"(
          (instance Foo of Bar
              (properties baz 1)
              (methods quux)
              (method (mu a b &temp c)
                (return)))
      )");

  EXPECT_TRUE(result.ok()) << result.status().messages()[0].primary().message();
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ClassDef>(_)));
}

TEST(ParseItemsTest, ClassDecl) {
  auto result = TryParseItems(R"(
          (classdef Foo
            script# 1
            class# 2
            super# -1
            file# "Hello"
              (properties baz 1)
              (methods quux))
      )");

  EXPECT_TRUE(result.ok()) << result.status().messages()[0].primary().message();
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<ClassDecl>(_)));
}

TEST(ParseItemsTest, Selectors) {
  auto result = TryParseItems(R"(
            (selectors
                foo 1
                bar 2
                -objID- 4096)
        )");

  EXPECT_TRUE(result.ok()) << result.status().messages()[0].primary().message();
  EXPECT_THAT(result.value(), ElementsAre(util::ChoiceOf<SelectorsDecl>(_)));
}

}  // namespace
}  // namespace parsers::sci
