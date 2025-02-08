#ifndef TOKENIZER_CHAR_STREAM_HPP
#define TOKENIZER_CHAR_STREAM_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "scic/tokenizer/text_contents.hpp"

namespace tokenizer {

class CharStream {
 public:
  CharStream() = default;
  CharStream(std::string_view input, std::size_t offset = 0,
             std::optional<std::size_t> end_offset = std::nullopt);

  CharStream& operator++();
  CharStream operator++(int);

  explicit operator bool() const;
  bool operator!() const;
  char operator*() const;

  bool AtStart() const;

  CharStream FindNext(char c) const;
  CharStream FindNextOf(std::string_view chars) const;

  CharStream SkipChar(char c) const;
  CharStream SkipCharsOf(std::string_view chars) const;
  CharStream SkipN(std::size_t n) const;

  CharOffset Offset() const;
  CharRange RangeTo(CharStream const& other) const;

  std::string_view GetTextTo(CharStream const& other) const;
  CharStream GetStreamTo(CharStream const& other) const;

  bool TryConsumePrefix(std::string_view prefix);

 private:
  CharStream(std::shared_ptr<TextContents> contents, std::size_t start_offset,
             std::size_t end_offset);
  struct LineSpan {
    std::size_t start;
    std::size_t end;
  };

  bool AtEnd() const;
  void Advance();
  std::string_view Remainder() const;
  std::size_t IndexOf(std::string_view) const;
  std::size_t IndexNotOf(std::string_view) const;

  std::shared_ptr<TextContents> contents_;
  std::size_t curr_index_;
  std::size_t end_index_;
};

}  // namespace tokenizer

#endif