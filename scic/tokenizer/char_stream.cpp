#include "scic/tokenizer/char_stream.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
namespace tokenizer {

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
