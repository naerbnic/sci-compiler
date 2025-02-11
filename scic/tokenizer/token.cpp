#include "scic/tokenizer/token.hpp"

#include <utility>

#include "scic/text/text_range.hpp"

namespace tokenizer {

Token::Token(text::TextRange text_range, TokenValue value)
    : text_range_(std::move(text_range)), value_(std::move(value)) {}

}  // namespace tokenizer