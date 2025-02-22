#ifndef TOKENIZER_CHAR_STREAM_HPP
#define TOKENIZER_CHAR_STREAM_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "scic/text/text_range.hpp"

namespace tokens {

class CharStream {
 public:
  CharStream() = default;
  CharStream(std::string text_range);
  CharStream(text::TextRange text_range);

  CharStream& operator++();
  CharStream operator++(int);

  explicit operator bool() const;
  char operator*() const;

  bool AtStart() const;

  CharStream FindNext(char c) const;
  CharStream FindNextOf(std::string_view chars) const;

  CharStream SkipChar(char c) const;
  CharStream SkipCharsOf(std::string_view chars) const;
  CharStream SkipN(std::size_t n) const;

  text::TextRange GetTextTo(CharStream const& other) const;
  CharStream GetStreamTo(CharStream const& other) const;

  bool TryConsumePrefix(std::string_view prefix);

  text::TextRange GetText() const { return range_; }

 private:
  struct LineSpan {
    std::size_t start;
    std::size_t end;
  };

  bool AtEnd() const;
  void Advance();
  std::string_view Remainder() const;
  std::size_t IndexOf(std::string_view) const;
  std::size_t IndexNotOf(std::string_view) const;

  text::TextRange range_;
};

}  // namespace tokens

#endif