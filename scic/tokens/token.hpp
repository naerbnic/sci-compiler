#ifndef TOKENIZER_TOKEN_HPP
#define TOKENIZER_TOKEN_HPP

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "scic/text/text_range.hpp"
#include "util/choice.hpp"

namespace tokens {

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

  class TokenValue : public util::ChoiceBase<TokenValue, Ident, String, Number,
                                             Punct, PreProcessor> {
    using ChoiceBase::ChoiceBase;
  };

  Token() = default;
  Token(text::TextRange text_range, TokenValue value);

  text::TextRange const& text_range() const { return text_range_; }
  TokenValue const& value() const { return value_; }

  Ident const* AsIdent() const { return value_.try_get<Ident>(); }
  Punct const* AsPunct() const { return value_.try_get<Punct>(); }
  Number const* AsNumber() const { return value_.try_get<Number>(); }
  String const* AsString() const { return value_.try_get<String>(); }
  PreProcessor const* AsPreProcessor() const {
    return value_.try_get<PreProcessor>();
  }

 private:
  text::TextRange text_range_;
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

}  // namespace tokens

#endif