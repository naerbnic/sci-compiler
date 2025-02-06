#include "scic/tokenizer/token.hpp"

#include <string>

namespace tokenizer {

Token::Token(CharRange char_range, std::string raw_text, TokenValue value)
    : char_range_(std::move(char_range)),
      raw_text_(std::move(raw_text)),
      value_(std::move(value)) {}

}  // namespace tokenizer