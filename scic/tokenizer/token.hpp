#ifndef TOKENIZER_TOKEN_HPP
#define TOKENIZER_TOKEN_HPP

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "scic/tokenizer/text_contents.hpp"

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
    PPT_ENDIF,
  };

  enum PunctType : char {
    PCT_HASH = '#',
    PCT_LPAREN = '(',
    PCT_RPAREN = ')',
    PCT_COMMA = ',',
    PCT_DOT = '.',
    PCT_AT = '@',
    PCT_LBRACKET = '[',
    PCT_RBRACKET = ']',
  };

  // Token subtypes.

  // An identifier.
  struct Ident {
    // The trailing character of the identifier.
    enum Trailer {
      None,
      // ':'
      Colon,
      // '?'
      Question,
    };
    // The name of the identifier. This may not exactly match the raw string
    // if some kind of escape sequence is used.
    std::string name;
    Trailer trailer;
  };

  // A Selector in a call (e.g. "mySelector:")
  struct Selector {
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

  using TokenValue = std::variant<Ident, String, Number, Punct, PreProcessor>;

  Token() = default;
  Token(CharRange file_range, std::string raw_text, TokenValue value);

  CharRange const& char_range() const { return char_range_; }
  std::string_view raw_text() const { return raw_text_; }
  TokenValue const& value() const { return value_; }

 private:
  CharRange char_range_;
  std::string raw_text_;
  TokenValue value_;
};

}  // namespace tokenizer

#endif