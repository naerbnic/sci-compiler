#include "scic/tokenizer/token_readers.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "scic/chartype.hpp"
#include "scic/tokenizer/token.hpp"

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

int CharDigitValue(char c, std::optional<uint8_t> base = std::nullopt) {
  c = absl::ascii_tolower(c);
  auto index = hexDigits.find(c);
  if (index == std::string_view::npos) {
    throw std::runtime_error("Invalid digit");
  }
  if (base && index >= *base) {
    throw std::runtime_error("Digit out of range");
  }
  return index;
}

}  // namespace

int ReadKey(CharStream& stream) {
  if (!stream) {
    throw std::runtime_error("End of stream reached while reading key");
  }

  int result;

  char currChar;
  switch ((currChar = *stream++)) {
    case '^': {
      if (!stream) {
        throw std::runtime_error("End of stream reached while reading key");
      }
      // A control key.
      auto ctrlChar = *stream++;
      if (isalpha(ctrlChar)) {
        result = toupper(ctrlChar) - 0x40;
      } else {
        // Error("Not a valid control key: %s", gSymStr);
      }
      break;
    }

    case '@': {
      if (!stream) {
        throw std::runtime_error("End of stream reached while reading key");
      }
      // An alt-key.
      auto altChar = *stream++;
      if (isalpha(altChar)) {
        result = altKey[toupper(altChar) - 'A'] << 8;
      } else {
        // Error("Not a valid alt key: %c", altChar);
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
      if (!absl::SimpleAtoi(start_pos.GetTextTo(stream), &num)) {
        // Error("Not a valid function key: %s", gSymStr);
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
    throw std::runtime_error("Extra characters after key");
  }
  return result;
}

std::optional<int> ReadNumber(CharStream& stream) {
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
    auto value = CharDigitValue(*stream, base);
    if (value >= base) {
      // Warning("Invalid character in number: %c.  Number = %d", rawChar,
      //         symVal());
      break;
    }
    val = (val * base) + value;
    ++stream;
  }

  val *= sign;

  return val;
}

std::optional<std::string> ReadString(CharStream& stream) {
  char open = *stream++;
  char close = (open == ALT_QUOTE) ? '}' : open;

  std::string parsed_string;
  bool truncated = false;
  while (stream && *stream != close) {
    char currChar = *stream++;
    switch (currChar) {
      case '\r':
        break;

      case '_':
        if (!truncated) parsed_string.push_back(' ');
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
          int high_digit = CharDigitValue(*stream++, 16);
          int low_digit = CharDigitValue(*stream++, 16);
          char c = (high_digit << 4) | (low_digit & 0x0F);
          if (!truncated) parsed_string.push_back(c);
        } else {
          // Then just use char as is.
          char c = *stream++;
          switch (c) {
            case 'n':
              if (!truncated) parsed_string.push_back('\n');
              break;
            case 't':
              if (!truncated) parsed_string.push_back('\t');
              break;
            case 'r':
              if (!truncated) {
                parsed_string.push_back('\r');
              }
              break;
            case '\\':
            case '"':
            case '}':
              if (!truncated) parsed_string.push_back(c);
              break;
            default:
              // Should we error on unexpected escape sequences?
              if (!truncated) parsed_string.push_back(c);
              break;
          }
        }

        break;
      }

      default:
        if (!truncated) parsed_string.push_back(currChar);
        break;
    }

    if (parsed_string.length() >= MaxTokenLen && !truncated) {
      // Error("String too large.");
      truncated = true;
    }
  }

  if (!stream) {
    // Error("Unterminated string");
  }

  if (*stream != close) {
    throw std::runtime_error("String finished with unexpected close char");
  }
  ++stream;

  return parsed_string;
}

std::optional<Token::Ident> ReadIdent(CharStream& stream) {
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

std::optional<Token::PreProcessor> ReadPreprocessor(CharStream& stream) {
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
    auto next_token = NextToken(curr_stream);
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
std::optional<Token::TokenValue> ReadToken(CharStream& stream) {
  if (IsTok(*stream)) {
    // This is equivalent to our Punct struct.
    return Token::Punct{
        .type = CharToPunctType(*stream),
    };
  }

  if (*stream == '`') {
    // A character constant.
    ++stream;
    int keyValue = ReadKey(stream);
    return Token::Number{
        .value = keyValue,
    };
  }

  if (*stream == '"' || *stream == ALT_QUOTE) {
    auto parsed_string = ReadString(stream);
    return Token::String{
        .decodedString = *parsed_string,
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
    auto number = ReadNumber(stream);
    if (!number) {
      return std::nullopt;
    }
    return Token::Number{
        .value = *number,
    };
  }

  // Read an identifier.

  return ReadIdent(stream);
}

std::optional<Token> NextToken(CharStream& stream) {
  bool at_start_of_line = stream.AtStart();

  while (true) {
    if (!stream) {
      return std::nullopt;
    }

    if (at_start_of_line) {
      auto start_of_line = stream;
      auto preprocessor = ReadPreprocessor(stream);
      if (preprocessor) {
        return Token(start_of_line.RangeTo(stream),
                     std::string(start_of_line.GetTextTo(stream)),
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
  auto token_value = ReadToken(stream);
  if (!token_value) {
    return std::nullopt;
  }

  std::string raw_text(token_start.GetTextTo(stream));
  auto char_range = token_start.RangeTo(stream);
  return Token(char_range, raw_text, *token_value);
}

}  // namespace tokenizer