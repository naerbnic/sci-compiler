#ifndef TOKENIZER_TOKEN_TEST_UTILS
#define TOKENIZER_TOKEN_TEST_UTILS

#include <string>
#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "scic/tokenizer/char_stream.hpp"
#include "scic/tokenizer/token.hpp"

namespace tokenizer {

struct IdentSpec {
  testing::Matcher<std::string> name = testing::_;
  testing::Matcher<Token::Ident::Trailer> trailer = Token::Ident::None;
};

inline testing::Matcher<Token::Ident const&> IdentOf(IdentSpec const& spec) {
  return testing::AllOf(
      testing::Field("name", &Token::Ident::name, spec.name),
      testing::Field("trailer", &Token::Ident::trailer, spec.trailer));
}

struct PreProcSpec {
  testing::Matcher<Token::PreProcessorType> type = testing::_;
  testing::Matcher<std::vector<Token>> lineTokens = testing::_;
};

inline testing::Matcher<Token::PreProcessor const&> PreProcOf(
    PreProcSpec const& spec) {
  return testing::AllOf(
      testing::Field("type", &Token::PreProcessor::type, spec.type),
      testing::Field("lineTokens", &Token::PreProcessor::lineTokens,
                     spec.lineTokens));
}

struct TokenSpec {
  testing::Matcher<CharRange const&> char_range = testing::_;
  testing::Matcher<std::string_view> raw_text = testing::_;
  testing::Matcher<Token::TokenValue const&> value = testing::_;
};

inline testing::Matcher<Token const&> TokenOf(TokenSpec const& spec) {
  return testing::AllOf(
      testing::Property("char_range", &Token::char_range, spec.char_range),
      testing::Property("raw_text", &Token::raw_text, spec.raw_text),
      testing::Property("value", &Token::value, spec.value));
}

inline testing::Matcher<Token const&> IdentTokenOf(
    testing::Matcher<std::string> name) {
  return TokenOf({
      .value = testing::VariantWith(IdentOf({
          .name = name,
      })),
  });
}

}  // namespace tokenizer

#endif