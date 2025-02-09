#ifndef TOKENIZER_TOKEN_HPP
#define TOKENIZER_TOKEN_HPP

#include <string>
#include <variant>
#include <vector>

#include "absl/strings/str_format.h"
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
  Token(TextRange text_range, TokenValue value);

  TextRange const& text_range() const { return text_range_; }
  TokenValue const& value() const { return value_; }

  Ident const* AsIdent() const { return std::get_if<Ident>(&value_); }
  Punct const* AsPunct() const { return std::get_if<Punct>(&value_); }
  Number const* AsNumber() const { return std::get_if<Number>(&value_); }
  String const* AsString() const { return std::get_if<String>(&value_); }
  PreProcessor const* AsPreProcessor() const {
    return std::get_if<PreProcessor>(&value_);
  }

 private:
  TextRange text_range_;
  TokenValue value_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, Token const& token) {
    if (auto* ident = token.AsIdent()) {
      absl::Format(&sink, "Ident(%v)", ident->name);
    } else if (auto* string = token.AsString()) {
      absl::Format(&sink, "String(%v)", string->decodedString);
    } else if (auto* number = token.AsNumber()) {
      absl::Format(&sink, "Number(%d)", number->value);
    } else if (auto* punct = token.AsPunct()) {
      absl::Format(&sink, "Punct(%d)", punct->type);
    } else if (auto* preproc = token.AsPreProcessor()) {
      absl::Format(&sink, "PreProc(%d)", preproc->type);
    } else {
      absl::Format(&sink, "Unknown");
    }
  }
};

}  // namespace tokenizer

#endif