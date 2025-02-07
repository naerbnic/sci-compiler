#ifndef TOKENIZER_TOKEN_READERS_HPP
#define TOKENIZER_TOKEN_READERS_HPP

#include <optional>

#include "absl/status/statusor.h"
#include "scic/tokenizer/token.hpp"

namespace tokenizer {
absl::StatusOr<std::optional<Token>> NextToken(CharStream& stream);

// These are generally internal, and are provided for testing.
absl::StatusOr<int> ReadKey(CharStream& stream);
absl::StatusOr<int> ReadNumber(CharStream& stream);
absl::StatusOr<std::string> ReadString(CharStream& stream);
absl::StatusOr<Token::Ident> ReadIdent(CharStream& stream);
absl::StatusOr<std::optional<Token::PreProcessor>> ReadPreprocessor(
    CharStream& stream);
absl::StatusOr<Token::TokenValue> ReadToken(CharStream& stream);

}  // namespace tokenizer

#endif