#include "scic/tokenizer/token_readers.hpp"

#include <gtest/gtest.h>

#include "gmock/gmock.h"
#include "scic/tokenizer/char_stream.hpp"
#include "scic/tokenizer/token.hpp"
#include "scic/tokenizer/token_test_utils.hpp"
#include "util/status/status_matchers.hpp"

namespace tokenizer {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::VariantWith;
using ::util::status::IsOkAndHolds;

// ReadKeyTest

TEST(ReadKeyTest, SimpleKeyWorks) {
  auto stream = CharStream("a");
  EXPECT_THAT(ReadKey(stream), IsOkAndHolds('a'));
  EXPECT_FALSE(stream);
}

TEST(ReadKeyTest, ControlKeyWorks) {
  auto stream = CharStream("^D");
  EXPECT_THAT(ReadKey(stream), IsOkAndHolds(4));
  EXPECT_FALSE(stream);
}

TEST(ReadKeyTest, AltKeyWorks) {
  auto stream = CharStream("@A");
  EXPECT_THAT(ReadKey(stream), IsOkAndHolds(7680));
  EXPECT_FALSE(stream);
}

TEST(ReadKeyTest, FnKeyWorks) {
  auto stream = CharStream("#123");
  EXPECT_THAT(ReadKey(stream), IsOkAndHolds(46336));
  EXPECT_FALSE(stream);
}

// ReadNumberTest

TEST(ReadNumberTest, DecWorks) {
  auto stream = CharStream("123");
  EXPECT_THAT(ReadNumber(stream), IsOkAndHolds(123));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, NegDecWorks) {
  auto stream = CharStream("-123");
  EXPECT_THAT(ReadNumber(stream), IsOkAndHolds(-123));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, BinWorks) {
  auto stream = CharStream("%00101");
  EXPECT_THAT(ReadNumber(stream), IsOkAndHolds(5));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, HexWorks) {
  auto stream = CharStream("$F0F0");
  EXPECT_THAT(ReadNumber(stream), IsOkAndHolds(0xF0F0));
  EXPECT_FALSE(stream);
}

TEST(ReadNumberTest, NegHexWorks) {
  auto stream = CharStream("-$FF");
  EXPECT_THAT(ReadNumber(stream), IsOkAndHolds(-255));
  EXPECT_FALSE(stream);
}

// ReadStringTest

TEST(ReadStringTest, SimpleStringWorks) {
  auto stream = CharStream(R"("foo")");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("foo"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, AltQuoteStringWorks) {
  auto stream = CharStream("{foo}");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("foo"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, NewlineEscapeWorks) {
  auto stream = CharStream(R"("foo\nbar")");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("foo\nbar"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, SpaceIsPreserved) {
  auto stream = CharStream(R"("foo bar")");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("foo bar"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, SpacesAreCollapsed) {
  auto stream = CharStream(R"("foo    bar")");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("foo bar"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, NewlinesAndTabsAreWhitespace) {
  auto stream = CharStream("{foo \t\n\t bar}");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("foo bar"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, ConvertsSingleCharacterEscapes) {
  auto stream = CharStream(R"("baz\n\r\t\"\}\\")");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("baz\n\r\t\"}\\"));
  EXPECT_FALSE(stream);
}

TEST(ReadStringTest, ConvertsHexCharacters) {
  auto stream = CharStream(R"("foo\61\6a\6Bbar")");
  EXPECT_THAT(ReadString(stream), IsOkAndHolds("fooajkbar"));
  EXPECT_FALSE(stream);
}

// ReadIdentTest

TEST(ReadIdentTest, BasicIdentWorks) {
  auto stream = CharStream("foo");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, IsOkAndHolds(IdentOf({
                         .name = "foo",
                         .trailer = Token::Ident::None,
                     })));
  EXPECT_FALSE(stream);
}

TEST(ReadIdentTest, ColonIdentWorks) {
  auto stream = CharStream("foo:");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, IsOkAndHolds(IdentOf({
                         .name = "foo",
                         .trailer = Token::Ident::Colon,
                     })));
  EXPECT_FALSE(stream);
}

TEST(ReadIdentTest, QuestionIdentWorks) {
  auto stream = CharStream("foo?");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, IsOkAndHolds(IdentOf({
                         .name = "foo",
                         .trailer = Token::Ident::Question,
                     })));
  EXPECT_FALSE(stream);
}

TEST(ReadIdentTest, SpecialCharsInIdentWork) {
  auto stream = CharStream("a-b_c<=>");
  auto value = ReadIdent(stream);
  EXPECT_THAT(value, IsOkAndHolds(IdentOf({
                         .name = "a-b_c<=>",
                         .trailer = Token::Ident::None,
                     })));
  EXPECT_FALSE(stream);
}

// ReadPreprocessorTest

// ReadTokenTest

TEST(ReadTokenTest, SimpleCase) {
  auto stream = CharStream("foo");
  EXPECT_THAT(ReadToken(stream), IsOkAndHolds(VariantWith(IdentOf({
                                     .name = "foo",
                                 }))));
  EXPECT_FALSE(stream);
}

// NextTokenTest

TEST(NextTokenTest, SimpleCase) {
  auto stream = CharStream("foo");
  EXPECT_THAT(NextToken(stream), IsOkAndHolds(Optional(TokenOf({
                                     .text_range = TextRangeOf("foo"),
                                     .value = VariantWith(IdentOf({
                                         .name = "foo",
                                     })),
                                 }))));
  EXPECT_FALSE(stream);
}

TEST(NextTokenTest, InitialWhitespaceIsSkipped) {
  auto stream = CharStream("  \n\tfoo");
  EXPECT_THAT(NextToken(stream), IsOkAndHolds(Optional(TokenOf({
                                     .text_range = TextRangeOf("foo"),
                                     .value = VariantWith(IdentOf({
                                         .name = "foo",
                                     })),
                                 }))));
  EXPECT_FALSE(stream);
}

TEST(NextTokenTest, PreProcessorDirectiveWorksOnFirstLine) {
  auto stream = CharStream("#if foo\nbar");
  EXPECT_THAT(NextToken(stream),
              IsOkAndHolds(Optional(TokenOf({
                  .text_range = TextRangeOf("#if foo"),
                  .value = VariantWith(PreProcOf({
                      .type = Token::PPT_IF,
                      .lineTokens = ElementsAre(IdentTokenOf("foo")),
                  })),
              }))));

  EXPECT_THAT(NextToken(stream), IsOkAndHolds(Optional(IdentTokenOf("bar"))));
  EXPECT_FALSE(stream);
}

TEST(NextTokenTest, PreProcessorDirectiveWorksOnAnotherLine) {
  auto stream = CharStream("    \n#if foo\nbar");
  EXPECT_THAT(NextToken(stream),
              IsOkAndHolds(Optional(TokenOf({
                  .text_range = TextRangeOf("#if foo"),
                  .value = VariantWith(PreProcOf({
                      .type = Token::PPT_IF,
                      .lineTokens = ElementsAre(IdentTokenOf("foo")),
                  })),
              }))));

  EXPECT_THAT(NextToken(stream), IsOkAndHolds(Optional(IdentTokenOf("bar"))));
  EXPECT_FALSE(stream);
}

}  // namespace
}  // namespace tokenizer