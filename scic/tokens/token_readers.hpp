#ifndef TOKENIZER_TOKEN_READERS_HPP
#define TOKENIZER_TOKEN_READERS_HPP

#include <optional>
#include <string>
#include <vector>

#include "scic/status/status.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/char_stream.hpp"
#include "scic/tokens/token.hpp"

namespace tokens {

status::StatusOr<std::optional<Token>> NextToken(CharStream& stream);
status::StatusOr<std::vector<Token>> TokenizeText(text::TextRange text);

// These are generally internal, and are provided for testing.
status::StatusOr<int> ReadKey(CharStream& stream);
status::StatusOr<int> ReadNumber(CharStream& stream);
status::StatusOr<std::string> ReadString(CharStream& stream);
status::StatusOr<Token::Ident> ReadIdent(CharStream& stream);
status::StatusOr<std::optional<Token::PreProcessor>> ReadPreprocessor(
    CharStream& stream);
status::StatusOr<Token::TokenValue> ReadToken(CharStream& stream);

}  // namespace tokens

#endif