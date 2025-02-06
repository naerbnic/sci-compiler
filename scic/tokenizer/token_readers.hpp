#ifndef TOKENIZER_TOKEN_READERS_HPP
#define TOKENIZER_TOKEN_READERS_HPP

#include <optional>

#include "scic/tokenizer/token.hpp"

namespace tokenizer {
std::optional<Token> NextToken(CharStream& stream);

// These are generally internal, and are provided for testing.
int ReadKey(CharStream& stream);
std::optional<int> ReadNumber(CharStream& stream);
std::optional<std::string> ReadString(CharStream& stream);
std::optional<Token::Ident> ReadIdent(CharStream& stream);
std::optional<Token::TokenValue> ReadToken(CharStream& stream);

}  // namespace tokenizer

#endif