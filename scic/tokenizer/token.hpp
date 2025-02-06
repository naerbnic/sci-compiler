#ifndef TOKENIZER_TOKEN_HPP
#define TOKENIZER_TOKEN_HPP

#include <string>
#include <variant>
#include <vector>

#include "scic/tokenizer/char_stream.hpp"

namespace tokenizer {

// A value type representing a single parsed token.
class Token {
 public:
  enum PreProcessorType {
    PPT_IFDEF,
    PPT_IFNDEF,
    PPT_IF,
    PPT_ELIFDEF,
    PPT_ELIFNDEF,
    PPT_ELIF,
    PPT_ELSE,
  };

  enum PunctType : char {
    OP_HASH = '#',
    OP_LPAREN = '(',
    OP_RPAREN = ')',
    OP_COMMA = ',',
    OP_DOT = '.',
    OP_AT = '@',
    OP_LBRACKET = '[',
    OP_RBRACKET = ']',
  };

  // Token subtypes.

  // An identifier.
  struct Ident {
    // The name of the identifier. This may not exactly match the raw string
    // if some kind of escape sequence is used.
    std::string name;
  };

  struct String {
    std::string decodedString;
  };

  struct Number {
    // The numerical value found for this token.
    // This is an int, as we want to preserve negative values.
    int value;
  };

  // Punctuation.
  struct Punct {
    // Which kind of punctuation this token is.
    PunctType type;
  };

  // A preprocessor directive.
  struct PreProcessor {
    // The kind of preprocessor directive.
    PreProcessorType type;
    // The tokens remaining on the same line as the directive.
    std::vector<Token> lineTokens;
  };

 private:
  using TokenValue = std::variant<Ident, String, Number, Punct, PreProcessor>;

 public:
  Token() = default;

 private:
  FileRange file_range_;
  std::string raw_text_;
  TokenValue value_;
};

}  // namespace tokenizer

#endif