#include "scic/tokenizer/token.hpp"

#include <utility>

#include "scic/tokenizer/text_contents.hpp"

namespace tokenizer {

Token::Token(TextRange text_range, TokenValue value)
    : text_range_(std::move(text_range)), value_(std::move(value)) {}

}  // namespace tokenizer