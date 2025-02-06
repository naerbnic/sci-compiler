#include "scic/tokenizer/char_stream.hpp"

#include <__algorithm/ranges_binary_search.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

#include "absl/strings/str_replace.h"

namespace tokenizer {

TextContents::TextContents() : TextContents("") {}

TextContents::TextContents(std::string_view contents)
    : contents_(absl::StrReplaceAll(contents, {
                                                  {"\r\n", "\n"},
                                                  {"\r", "\n"},
                                              })) {
  std::size_t line_start_index = 0;
  while (true) {
    auto newline = contents_.find('\n', line_start_index);
    if (newline == std::string_view::npos) {
      break;
    }
    line_spans_.push_back({
        .start = line_start_index,
        .end = newline,
    });
    line_start_index = newline + 1;
  }
  line_spans_.push_back({
      .start = line_start_index,
      .end = contents_.size(),
  });
}

std::string_view TextContents::GetLine(std::size_t line_index) const {
  if (line_index >= line_spans_.size()) {
    throw std::out_of_range("Line index out of range.");
  }
  auto const& line = line_spans_[line_index];
  return std::string_view(contents_).substr(line.start, line.end - line.start);
}

char TextContents::CharAt(std::size_t byte_offset) const {
  if (byte_offset >= contents_.size()) {
    throw std::out_of_range("Byte offset out of range.");
  }
  return contents_[byte_offset];
}

CharOffset TextContents::GetOffset(std::size_t byte_offset) const {
  auto lower_bound_iter =
      std::lower_bound(line_spans_.begin(), line_spans_.end(), byte_offset,
                       [](LineSpan const line, std::size_t byte_offset) {
                         return line.end < byte_offset;
                       });
  auto line_index = lower_bound_iter - line_spans_.begin();
  auto line_offset = byte_offset - lower_bound_iter->start;
  return CharOffset(byte_offset, line_index, line_offset);
}

CharStream::CharStream(std::string_view input) : contents_(input) {}

CharStream& CharStream::operator++() {
  if (AtEnd()) {
    throw std::runtime_error("Incrementing past end of input.");
  }
  curr_index_++;
  return *this;
}

CharStream CharStream::operator++(int) {
  if (AtEnd()) {
    throw std::runtime_error("Incrementing past end of input.");
  }
  CharStream old = *this;
  curr_index_++;
  return old;
}

CharStream::operator bool() const { return !AtEnd(); }
bool CharStream::operator!() const { return AtEnd(); }
char CharStream::operator*() const {
  if (AtEnd()) {
    throw std::runtime_error("Dereferencing end of input.");
  }
  return contents_.CharAt(curr_index_);
}

CharStream CharStream::FindNext(char c) const {
  return FindNextOf(std::string_view(&c, 1));
}

CharStream CharStream::FindNextOf(std::string_view chars) const {
  auto copy = *this;
  auto pos = contents_.contents().find_first_of(chars, curr_index_);
  if (pos == std::string_view::npos) {
    copy.curr_index_ = contents_.size();
  } else {
    copy.curr_index_ = pos;
  }
  return copy;
}

CharStream CharStream::SkipChar(char c) const {
  return SkipCharsOf(std::string_view(&c, 1));
}

CharStream CharStream::SkipCharsOf(std::string_view chars) const {
  auto copy = *this;
  auto pos = contents_.contents().find_first_not_of(chars, curr_index_);
  if (pos == std::string_view::npos) {
    copy.curr_index_ = contents_.size();
  } else {
    copy.curr_index_ = pos;
  }
  return copy;
}

CharStream CharStream::SkipN(std::size_t n) const {
  auto copy = *this;
  if (curr_index_ + n >= contents_.size()) {
    throw std::runtime_error("Skipping past end of input.");
  }
  copy.curr_index_ += n;
  return copy;
}

CharOffset CharStream::Offset() const {
  return contents_.GetOffset(curr_index_);
}

CharRange CharStream::RangeTo(CharStream const& other) const {
  return CharRange(Offset(), other.Offset());
}

bool CharStream::AtEnd() const { return contents_.size() == curr_index_; }

}  // namespace tokenizer
