#include "scic/tokens/token.hpp"

#include <utility>

#include "scic/text/text_range.hpp"

namespace tokens {

Token::Token(text::TextRange text_range, TokenValue value)
    : text_range_(std::move(text_range)), value_(std::move(value)) {}

}  // namespace tokens