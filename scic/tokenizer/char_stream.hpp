#ifndef TOKENIZER_CHAR_STREAM_HPP
#define TOKENIZER_CHAR_STREAM_HPP

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tokenizer {

class CharOffset {
 public:
  CharOffset() = default;
  CharOffset(std::size_t byte_offset, std::size_t line_index,
             std::size_t column_number)
      : byte_offset_(byte_offset),
        line_index_(line_index),
        column_index_(column_number) {}

  std::size_t byte_offset() const { return byte_offset_; }
  std::size_t line_index() const { return line_index_; }
  std::size_t column_index() const { return column_index_; }

 private:
  std::size_t byte_offset_;
  std::size_t line_index_;
  std::size_t column_index_;
};

class CharRange {
 public:
  CharRange() = default;
  CharRange(CharOffset start, CharOffset end) : start_(start), end_(end) {}

  CharOffset const& start() const { return start_; }
  CharOffset const& end() const { return end_; }

 private:
  CharOffset start_;
  CharOffset end_;
};

class FileRange {
 public:
  FileRange() = default;
  FileRange(std::shared_ptr<std::string> filename, CharRange range)
      : filename_(std::move(filename)), range_(range) {}

  std::string_view filename() const { return *filename_; }
  CharRange const& range() const { return range_; }
  CharOffset const& start() const { return range_.start(); }
  CharOffset const& end() const { return range_.end(); }

 private:
  std::shared_ptr<std::string> filename_;
  CharRange range_;
};

class TextContents {
 public:
  TextContents();
  TextContents(std::string_view contents);

  std::size_t size() const { return contents_.size(); }
  std::string_view contents() const { return contents_; }
  std::size_t num_lines() const { return line_spans_.size(); }
  std::string_view GetLine(std::size_t line_index) const;
  std::string_view GetBetween(std::size_t start_offset,
                              std::size_t end_offset) const;
  char CharAt(std::size_t byte_offset) const;
  CharOffset GetOffset(std::size_t byte_offset) const;

 private:
  struct LineSpan {
    std::size_t start;
    std::size_t end;
  };

  std::string contents_;
  std::vector<LineSpan> line_spans_;
};

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