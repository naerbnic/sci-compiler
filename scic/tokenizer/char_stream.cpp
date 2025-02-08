#include "scic/tokenizer/char_stream.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"

namespace tokenizer {
namespace {
std::optional<std::pair<std::size_t, std::size_t>> FindNextNewline(
    std::string_view contents, std::size_t start) {
  auto newline_start = contents.find_first_of("\n\r", start);
  if (newline_start == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t newline_end;
  auto rest = contents.substr(newline_start);
  if (rest.starts_with("\r\n")) {
    newline_end = newline_start + 2;
  } else {
    newline_end = newline_start + 1;
  }
  return std::make_pair(newline_start, newline_end);
}
}  // namespace

TextContents::TextContents() : TextContents("") {}

TextContents::TextContents(std::string_view contents)
    : contents_(std::string(contents)) {
  std::size_t line_start_index = 0;
  while (auto newline = FindNextNewline(contents_, line_start_index)) {
    line_spans_.push_back({
        .start = line_start_index,
        .end = newline->first,
    });
    line_start_index = newline->second;
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

std::string_view TextContents::GetBetween(std::size_t start_offset,
                                          std::size_t end_offset) const {
  if (start_offset > contents_.size() || end_offset > contents_.size()) {
    throw std::out_of_range("Byte offset out of range.");
  }
  return std::string_view(contents_).substr(start_offset,
                                            end_offset - start_offset);
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
  // In the unlikely event that the byte offset is in the middle of a
  // newline sequence, adjust up to the beginning of the next line.
  if (lower_bound_iter->start > byte_offset) {
    byte_offset = lower_bound_iter->start;
  }
  auto line_index = lower_bound_iter - line_spans_.begin();
  auto line_offset = byte_offset - lower_bound_iter->start;
  return CharOffset(byte_offset, line_index, line_offset);
}

CharStream::CharStream(std::string_view input, std::size_t offset,
                       std::optional<std::size_t> end_offset)
    : contents_(std::make_shared<TextContents>(input)),
      curr_index_(offset),
      end_index_(end_offset.value_or(input.size())) {}

CharStream& CharStream::operator++() {
  Advance();
  return *this;
}

CharStream CharStream::operator++(int) {
  CharStream old = *this;
  Advance();
  return old;
}

CharStream::operator bool() const { return !AtEnd(); }
bool CharStream::operator!() const { return AtEnd(); }
char CharStream::operator*() const {
  if (AtEnd()) {
    throw std::runtime_error("Dereferencing end of input.");
  }
  auto remainder = Remainder();
  if (remainder[0] == '\r') {
    return '\n';
  } else {
    return remainder[0];
  }
}

bool CharStream::AtStart() const { return curr_index_ == 0; }

CharStream CharStream::FindNext(char c) const {
  return FindNextOf(std::string_view(&c, 1));
}

CharStream CharStream::FindNextOf(std::string_view chars) const {
  std::string scratch;
  if (absl::StrContains(chars, '\n')) {
    scratch = absl::StrCat("\r", chars);
    chars = scratch;
  }
  auto copy = *this;
  copy.curr_index_ = IndexOf(chars);
  return copy;
}

CharStream CharStream::SkipChar(char c) const {
  return SkipCharsOf(std::string_view(&c, 1));
}

CharStream CharStream::SkipCharsOf(std::string_view chars) const {
  std::string scratch;
  if (absl::StrContains(chars, '\n')) {
    scratch = absl::StrCat("\r", chars);
    chars = scratch;
  }
  auto copy = *this;
  copy.curr_index_ = IndexNotOf(chars);
  return copy;
}

CharStream CharStream::SkipN(std::size_t n) const {
  auto copy = *this;
  if (curr_index_ + n >= end_index_) {
    throw std::runtime_error("Skipping past end of input.");
  }
  copy.curr_index_ += n;
  return copy;
}

CharOffset CharStream::Offset() const {
  return contents_->GetOffset(curr_index_);
}

CharRange CharStream::RangeTo(CharStream const& other) const {
  if (contents_.get() != other.contents_.get()) {
    throw std::runtime_error("Getting text range from different contents.");
  }
  if (end_index_ != other.end_index_) {
    throw std::runtime_error("Getting text range from different contents.");
  }
  return CharRange(Offset(), other.Offset());
}

std::string_view CharStream::GetTextTo(CharStream const& other) const {
  if (contents_.get() != other.contents_.get()) {
    throw std::runtime_error("Getting text range from different contents.");
  }
  if (end_index_ != other.end_index_) {
    throw std::runtime_error("Getting text range from different contents.");
  }

  if (curr_index_ > other.curr_index_) {
    throw std::runtime_error("Getting stream range in reverse.");
  }

  return Remainder().substr(0, other.curr_index_ - curr_index_);
}

CharStream CharStream::GetStreamTo(CharStream const& other) const {
  if (contents_.get() != other.contents_.get()) {
    throw std::runtime_error("Getting stream range from different contents.");
  }
  if (end_index_ != other.end_index_) {
    throw std::runtime_error("Getting stream range from different contents.");
  }

  if (curr_index_ > other.curr_index_) {
    throw std::runtime_error("Getting stream range in reverse.");
  }

  return CharStream(contents_, curr_index_, other.curr_index_);
}

bool CharStream::TryConsumePrefix(std::string_view prefix) {
  if (!Remainder().starts_with(prefix)) {
    return false;
  }
  curr_index_ += prefix.size();
  return true;
}

CharStream::CharStream(std::shared_ptr<TextContents> contents,
                       std::size_t start_offset, std::size_t end_offset)
    : contents_(contents), curr_index_(start_offset), end_index_(end_offset) {}

bool CharStream::AtEnd() const { return end_index_ == curr_index_; }
void CharStream::Advance() {
  if (AtEnd()) {
    throw std::runtime_error("Advancing past end of input.");
  }
  if (Remainder().starts_with("\r\n")) {
    curr_index_ += 2;
  } else {
    curr_index_ += 1;
  }
}

std::string_view CharStream::Remainder() const {
  return contents_->GetBetween(curr_index_, end_index_);
}

std::size_t CharStream::IndexOf(std::string_view chars) const {
  auto pos = Remainder().find_first_of(chars);
  if (pos == std::string_view::npos) {
    return end_index_;
  }
  return curr_index_ + pos;
}

std::size_t CharStream::IndexNotOf(std::string_view chars) const {
  auto pos = Remainder().find_first_not_of(chars);
  if (pos == std::string_view::npos) {
    return end_index_;
  }
  return curr_index_ + pos;
}

}  // namespace tokenizer
