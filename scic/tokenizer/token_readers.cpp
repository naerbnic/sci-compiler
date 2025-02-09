#include "scic/tokenizer/token_readers.hpp"

#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "scic/chartype.hpp"
#include "scic/tokenizer/char_stream.hpp"
#include "scic/tokenizer/text_contents.hpp"
#include "scic/tokenizer/token.hpp"
#include "util/status/status_macros.hpp"

namespace tokenizer {

namespace {

static constexpr std::string_view hexDigits = "0123456789abcdef";
static const uint32_t altKey[] = {
    30, 48, 46, 32, 18, 33, 34, 35, 23,  // a - i
    36, 37, 38, 50, 49, 24, 25, 16, 19,  // j - r
    31, 20, 22, 47, 17, 45, 21, 44       // s - z
};

constexpr int MaxTokenLen = 2048;

inline constexpr char ALT_QUOTE = '{';

Token::PunctType CharToPunctType(char c) {
  switch (c) {
    case '#':
      return Token::PCT_HASH;
    case '(':
      return Token::PCT_LPAREN;
    case ')':
      return Token::PCT_RPAREN;
    case ',':
      return Token::PCT_COMMA;
    case '.':
      return Token::PCT_DOT;
    case '@':
      return Token::PCT_AT;
    case '[':
      return Token::PCT_LBRACKET;
    case ']':
      return Token::PCT_RBRACKET;
    default:
      throw std::runtime_error("Invalid punctuation character");
  }
}

absl::Status ExpectNonEmpty(CharStream& stream) {
  if (!stream) {
    return absl::FailedPreconditionError("Unexpected end of stream");
  }
  return absl::OkStatus();
}

#define CHECK_NONEMPTY(stream) RETURN_IF_ERROR(ExpectNonEmpty(stream))

template <class... Args>
absl::Status TokenError(CharStream const& stream,
                        absl::FormatSpec<Args...> const& spec,
                        Args const&... args) {
  return absl::FailedPreconditionError(absl::StrFormat(spec, args...));
}

absl::StatusOr<int> CharDigitValue(CharStream& stream, uint8_t base) {
  char c = absl::ascii_tolower(*stream);
  auto index = hexDigits.find(c);
  if (index == std::string_view::npos) {
    return TokenError(stream, "Invalid digit: %c", c);
  }
  if (index >= base) {
    return TokenError(stream, "Invalid digit: %c", c);
  }
  stream++;
  return index;
}

}  // namespace

absl::StatusOr<int> ReadKey(CharStream& stream) {
  CHECK_NONEMPTY(stream);

  int result;

  char currChar;
  switch ((currChar = *stream++)) {
    case '^': {
      CHECK_NONEMPTY(stream);

      // A control key.
      auto ctrlChar = *stream++;
      if (isalpha(ctrlChar)) {
        result = toupper(ctrlChar) - 0x40;
      } else {
        return TokenError(stream, "Not a valid control key: %c", ctrlChar);
      }
      break;
    }

    case '@': {
      CHECK_NONEMPTY(stream);
      // An alt-key.
      auto altChar = *stream++;
      if (isalpha(altChar)) {
        result = altKey[toupper(altChar) - 'A'] << 8;
      } else {
        return TokenError(stream, "Not a valid alt key: %c", altChar);
      }
      break;
    }

    case '#': {
      // A function key.
      auto start_pos = stream;
      while (stream && !IsTerm(*stream)) {
        ++stream;
      }
      int num;
      if (!absl::SimpleAtoi(start_pos.GetTextTo(stream).contents(), &num)) {
        return TokenError(start_pos, "Not a valid function key: %s",
                          start_pos.GetTextTo(stream).contents());
        break;
      }
      result = (num + 58) << 8;
      break;
    }

    default:
      // Just a character...
      result = currChar;
      break;
  }

  // Check that we have reached the end of the key.
  //
  // The original code extracted all of the IsTerm characters before further
  // parsing, but that can cause some of the above cases not to process all
  // of the characters read. This attempts to fix that.
  if (stream && !IsTerm(*stream)) {
    return TokenError(stream, "Extra characters after key");
  }
  return result;
}

absl::StatusOr<int> ReadNumber(CharStream& stream) {
  // Determine the sign of the number
  int sign;
  if (*stream != '-')
    sign = 1;
  else {
    sign = -1;
    ++stream;
  }

  // Determine the base of the number
  int base = 10;
  if (*stream == '%' || *stream == '$') {
    base = (*stream++ == '%') ? 2 : 16;
  }

  int val = 0;
  while (stream && !IsTerm(*stream)) {
    ASSIGN_OR_RETURN(auto value, CharDigitValue(stream, base));
    val = (val * base) + value;
  }

  val *= sign;

  return val;
}

absl::StatusOr<std::string> ReadString(CharStream& stream) {
  char open = *stream++;
  char close = (open == ALT_QUOTE) ? '}' : open;

  std::string parsed_string;
  while (stream && *stream != close) {
    char currChar = *stream++;
    switch (currChar) {
      case '\r':
        break;

      case '_':
        parsed_string.push_back(' ');
        break;

      case ' ':
      case '\t':
      case '\n':
        parsed_string.push_back(' ');
        stream = stream.SkipCharsOf(" \t\n");
        break;

      case '\\': {
        if (IsHex(*stream)) {  // else move to next char
          // Then insert a hex number in the string.
          ASSIGN_OR_RETURN(int high_digit, CharDigitValue(stream, 16));
          ASSIGN_OR_RETURN(int low_digit, CharDigitValue(stream, 16));
          char c = (high_digit << 4) | (low_digit & 0x0F);
          parsed_string.push_back(c);
        } else {
          // Then just use char as is.
          char c = *stream++;
          switch (c) {
            case 'n':
              parsed_string.push_back('\n');
              break;
            case 't':
              parsed_string.push_back('\t');
              break;
            case 'r':
              parsed_string.push_back('\r');
              break;
            case '\\':
            case '"':
            case '}':
              parsed_string.push_back(c);
              break;
            default:
              return TokenError(stream, "Unexpected escape sequence: '\\%c'",
                                c);
              break;
          }
        }

        break;
      }

      default:
        parsed_string.push_back(currChar);
        break;
    }

    if (parsed_string.length() >= MaxTokenLen) {
      return TokenError(stream, "String too large.");
    }
  }

  if (!stream) {
    return TokenError(stream, "Unterminated string");
  }

  if (*stream != close) {
    throw std::runtime_error("String finished with unexpected close char");
  }
  ++stream;

  return parsed_string;
}

absl::StatusOr<Token::Ident> ReadIdent(CharStream& stream) {
  std::string ident;
  Token::Ident::Trailer trailer = Token::Ident::None;
  while (stream && !IsTerm(*stream)) {
    if (*stream == ':') {
      trailer = Token::Ident::Colon;
      ++stream;
      break;
    }

    if (*stream == '?') {
      trailer = Token::Ident::Question;
      ++stream;
      break;
    }

    ident.push_back(*stream++);
  }
  return Token::Ident{
      .name = std::move(ident),
      .trailer = trailer,
  };
}

absl::StatusOr<std::optional<Token::PreProcessor>> ReadPreprocessor(
    CharStream& stream) {
  // We should be at the beginning of a line. Skip over any whitespace.
  auto curr_stream = stream.SkipCharsOf(" \t");

  if (!curr_stream || *curr_stream != '#') {
    return std::nullopt;
  }

  struct PreProcDirective {
    std::string_view text;
    Token::PreProcessorType type;
  };

  PreProcDirective directives[] = {
      {"#ifdef", Token::PPT_IFDEF},
      {"#ifndef", Token::PPT_IFNDEF},
      {"#if", Token::PPT_IF},
      {"#elifdef", Token::PPT_ELIFDEF},
      {"#elifndef", Token::PPT_ELIFNDEF},
      {"#elif", Token::PPT_ELIF},
      {"#else", Token::PPT_ELSE},
      {"#endif", Token::PPT_ENDIF},
  };

  std::optional<Token::PreProcessorType> directive_type = std::nullopt;

  for (auto const& directive : directives) {
    if (curr_stream.TryConsumePrefix(directive.text)) {
      directive_type = directive.type;
      break;
    }
  }

  if (!directive_type) {
    return std::nullopt;
  }

  // We need to have a separator between the directive and the next token.
  if (curr_stream && !IsTerm(*curr_stream)) {
    return std::nullopt;
  }

  // We know we have a preproc directive. We grab the rest of the line
  // for the directive, and set our output stream to the end of the line.
  auto end_of_line_stream = curr_stream.FindNext('\n');
  curr_stream = curr_stream.GetStreamTo(end_of_line_stream);
  stream = end_of_line_stream;

  std::vector<Token> tokens;
  while (true) {
    ASSIGN_OR_RETURN(auto next_token, NextToken(curr_stream));
    if (!next_token) {
      break;
    }
    tokens.push_back(*next_token);
  }

  return Token::PreProcessor{
      .type = *directive_type,
      .lineTokens = std::move(tokens),
  };
}

// Read a token, assuming that the current stream location is at the
// start of the token.
absl::StatusOr<Token::TokenValue> ReadToken(CharStream& stream) {
  if (IsTok(*stream)) {
    // This is equivalent to our Punct struct.
    return Token::Punct{
        .type = CharToPunctType(*stream++),
    };
  }

  if (*stream == '`') {
    // A character constant.
    ++stream;
    ASSIGN_OR_RETURN(int keyValue, ReadKey(stream));
    return Token::Number{
        .value = keyValue,
    };
  }

  if (*stream == '"' || *stream == ALT_QUOTE) {
    ASSIGN_OR_RETURN(auto parsed_string, ReadString(stream));
    return Token::String{
        .decodedString = parsed_string,
    };
  }

  bool is_num = IsDigit(*stream);

  // We could also have a negative number.
  if (!is_num && *stream == '-') {
    auto next = stream;
    next++;
    is_num = next && IsDigit(*next);
  }

  if (is_num) {
    ASSIGN_OR_RETURN(auto number, ReadNumber(stream));
    return Token::Number{
        .value = number,
    };
  }

  // Read an identifier.

  return ReadIdent(stream);
}

absl::StatusOr<std::optional<Token>> NextToken(CharStream& stream) {
  bool at_start_of_line = stream.AtStart();

  while (true) {
    if (!stream) {
      return std::nullopt;
    }

    if (at_start_of_line) {
      auto start_of_line = stream;
      ASSIGN_OR_RETURN(auto preprocessor, ReadPreprocessor(stream));
      if (preprocessor) {
        return Token(start_of_line.GetTextTo(stream),
                     Token::TokenValue(*preprocessor));
      }
      at_start_of_line = false;
    }

    if (*stream == '\0') {
      throw std::runtime_error("Unexpected null character in input");
    }

    if (*stream == '\n') {
      at_start_of_line = true;
      ++stream;
      continue;
    }

    if (!IsSep(*stream)) break;

    stream = stream.SkipCharsOf(" \t");
    if (stream && *stream == ';') {
      stream = stream.FindNext('\n');
      continue;
    }
  }

  auto token_start = stream;
  ASSIGN_OR_RETURN(auto token_value, ReadToken(stream));
  return Token(token_start.GetTextTo(stream), token_value);
}

absl::StatusOr<std::vector<Token>> TokenizeText(TextRange text) {
  std::vector<Token> tokens;
  CharStream stream(std::move(text));
  while (true) {
    ASSIGN_OR_RETURN(auto token, NextToken(stream));
    if (!token) {
      break;
    }
    tokens.push_back(*token);
  }
  return tokens;
}

}  // namespace tokenizer