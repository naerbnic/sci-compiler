#include "scic/tokenizer/char_stream.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "scic/tokenizer/text_contents.hpp"
namespace tokenizer {

CharStream::CharStream(std::string input)
    : range_(TextRange::OfString(std::move(input))) {}
CharStream::CharStream(TextRange input) : range_(std::move(input)) {}

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

bool CharStream::AtStart() const { return range_.AtStart(); }

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
  copy.range_.RemovePrefix(IndexOf(chars));
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
  copy.range_.RemovePrefix(IndexNotOf(chars));
  return copy;
}

CharStream CharStream::SkipN(std::size_t n) const {
  auto copy = *this;
  if (n >= range_.size()) {
    throw std::runtime_error("Skipping past end of input.");
  }
  copy.range_.RemovePrefix(n);
  return copy;
}

TextRange CharStream::GetTextTo(CharStream const& other) const {
  return range_.GetPrefixTo(other.range_);
}

CharStream CharStream::GetStreamTo(CharStream const& other) const {
  return CharStream(GetTextTo(other));
}

bool CharStream::TryConsumePrefix(std::string_view prefix) {
  if (!Remainder().starts_with(prefix)) {
    return false;
  }
  range_.RemovePrefix(prefix.size());
  return true;
}

bool CharStream::AtEnd() const { return range_.size() == 0; }
void CharStream::Advance() {
  if (AtEnd()) {
    throw std::runtime_error("Advancing past end of input.");
  }
  if (Remainder().starts_with("\r\n")) {
    range_.RemovePrefix(2);
  } else {
    range_.RemovePrefix(1);
  }
}

std::string_view CharStream::Remainder() const { return range_.contents(); }

std::size_t CharStream::IndexOf(std::string_view chars) const {
  auto pos = Remainder().find_first_of(chars);
  if (pos == std::string_view::npos) {
    return range_.size();
  }
  return pos;
}

std::size_t CharStream::IndexNotOf(std::string_view chars) const {
  auto pos = Remainder().find_first_not_of(chars);
  if (pos == std::string_view::npos) {
    return range_.size();
  }
  return pos;
}

}  // namespace tokenizer
