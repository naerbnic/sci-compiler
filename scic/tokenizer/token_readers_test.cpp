#include "scic/tokenizer/token_readers.hpp"

#include <gtest/gtest.h>

#include "gmock/gmock.h"

namespace tokenizer {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Optional;

// ReadKeyTest

TEST(ReadKeyTest, SimpleKeyWorks) {
  auto stream = CharStream("a");
  auto value = ReadKey(stream);
  EXPECT_EQ(value, 'a');
  EXPECT_FALSE(stream);
}

TEST(ReadKeyTest, ControlKeyWorks) {
  auto stream = CharStream("^D");
  auto value = ReadKey(stream);
  EXPECT_EQ(value, 4);
  EXPECT_FALSE(stream);
}

TEST(ReadKeyTest, AltKeyWorks) {
  auto stream = CharStream("@A");
  auto value = ReadKey(stream);
  EXPECT_EQ(value, 7680);
  EXPECT_FALSE(stream);
}

TEST(ReadKeyTest, FnKeyWorks) {
  auto stream = CharStream("#123");
  auto value = ReadKey(stream);
  EXPECT_EQ(value, 46336);
  EXPECT_FALSE(stream);
}

// ReadNumberTest

TEST(ReadNumberTest, DecWorks) {
  auto stream = CharStream("123");
  EXPECT_THAT(ReadNumber(stream), Optional(123));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, NegDecWorks) {
  auto stream = CharStream("-123");
  EXPECT_THAT(ReadNumber(stream), Optional(-123));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, BinWorks) {
  auto stream = CharStream("%00101");
  EXPECT_THAT(ReadNumber(stream), Optional(5));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, HexWorks) {
  auto stream = CharStream("$F0F0");
  EXPECT_THAT(ReadNumber(stream), Optional(0xF0F0));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, NegHexWorks) {
  auto stream = CharStream("-$FF");
  EXPECT_THAT(ReadNumber(stream), Optional(-255));
  EXPECT_FALSE(stream);
}

// ReadStringTest

TEST(ReadStringTest, SimpleStringWorks) {
  auto stream = CharStream(R"("foo")");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, AltQuoteStringWorks) {
  auto stream = CharStream("{foo}");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, NewlineEscapeWorks) {
  auto stream = CharStream(R"("foo\nbar")");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo\nbar");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, SpaceIsPreserved) {
  auto stream = CharStream(R"("foo bar")");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo bar");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, SpacesAreCollapsed) {
  auto stream = CharStream(R"("foo    bar")");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo bar");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, NewlinesAndTabsAreWhitespace) {
  auto stream = CharStream("{foo \t\n\t bar}");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "foo bar");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, ConvertsSingleCharacterEscapes) {
  auto stream = CharStream(R"("baz\n\r\t\"\}\\")");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "baz\n\r\t\"}\\");
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, ConvertsHexCharacters) {
  auto stream = CharStream(R"("foo\61\6a\6Bbar")");
  auto value = ReadString(stream);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "fooajkbar");
  EXPECT_FALSE(stream);
}

// ReadIdentTest

testing::Matcher<Token::Ident const&> IdentOf(const std::string& name,
                                              Token::Ident::Trailer trailer) {
  return AllOf(Field("name", &Token::Ident::name, name),
               Field("trailer", &Token::Ident::trailer, trailer));
}

TEST(ReadIdentTest, BasicIdentWorks) {
  auto stream = CharStream("foo");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, Optional(IdentOf("foo", Token::Ident::None)));
  EXPECT_FALSE(stream);
}

TEST(ReadIdentTest, ColonIdentWorks) {
  auto stream = CharStream("foo:");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, Optional(IdentOf("foo", Token::Ident::Colon)));
  EXPECT_FALSE(stream);
}

TEST(ReadIdentTest, QuestionIdentWorks) {
  auto stream = CharStream("foo?");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, Optional(IdentOf("foo", Token::Ident::Question)));
  EXPECT_FALSE(stream);
}

TEST(ReadIdentTest, SpecialCharsInIdentWork) {
  auto stream = CharStream("a-b_c<=>");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, Optional(IdentOf("a-b_c<=>", Token::Ident::None)));
  EXPECT_FALSE(stream);
}

}  // namespace
}  // namespace tokenizer