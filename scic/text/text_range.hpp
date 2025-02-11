#ifndef TEXT_TEXT_RANGE_HPP
#define TEXT_TEXT_RANGE_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"

namespace text {

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

  template <class Sink>
  friend void AbslStringify(Sink& sink, FileRange const& range) {
    absl::Format(&sink, "%s:%d:%d-%d:%d", range.filename(),
                 range.start().line_index() + 1,
                 range.start().column_index() + 1, range.end().line_index() + 1,
                 range.end().column_index() + 1);
  }
};

class TextContents {
 public:
  TextContents(std::string contents);
  TextContents(std::string filename, std::string contents);

  std::size_t size() const { return contents_.size(); }
  std::shared_ptr<std::string> const& filename() const { return filename_; }
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

  std::shared_ptr<std::string> filename_;
  std::string contents_;
  std::vector<LineSpan> line_spans_;
};

class TextRange {
 public:
  static TextRange OfString(std::string contents);
  static TextRange WithFilename(std::string filename, std::string contents);

  TextRange() = default;
  ~TextRange();

  std::size_t size() const { return end_offset_ - start_offset_; }
  std::string_view contents() const {
    return contents_->GetBetween(start_offset_, end_offset_);
  }

  char CharAt(std::size_t byte_offset) const {
    if (byte_offset >= size()) {
      throw std::out_of_range("Byte offset out of range.");
    }
    return contents_->CharAt(start_offset_ + byte_offset);
  }

  CharOffset GetOffset(std::size_t byte_offset) const {
    if (byte_offset >= size()) {
      throw std::out_of_range("Byte offset out of range.");
    }
    return contents_->GetOffset(start_offset_ + byte_offset);
  }

  TextRange SubRange(std::size_t start,
                     std::optional<std::size_t> end = std::nullopt) const {
    std::size_t end_offset = end.value_or(size());
    if (start > size() || end_offset > size()) {
      throw std::out_of_range("Byte offset out of range.");
    }
    return TextRange(contents_, start_offset_ + start,
                     start_offset_ + end_offset);
  }

  void RemovePrefix(std::size_t num_bytes) {
    if (num_bytes > size()) {
      throw std::out_of_range("Byte offset out of range.");
    }
    start_offset_ += num_bytes;
  }

  FileRange GetRange() const {
    return FileRange(contents_->filename(),
                     CharRange(contents_->GetOffset(start_offset_),
                               contents_->GetOffset(end_offset_)));
  }

  bool SharesContentsWith(TextRange const& other) const {
    return contents_.get() == other.contents_.get();
  }

  TextRange GetPrefixTo(TextRange const& other) const {
    if (!SharesContentsWith(other)) {
      throw std::runtime_error("Getting text range from different contents.");
    }
    if (end_offset_ != other.end_offset_) {
      throw std::runtime_error("Ends do not match");
    }
    if (start_offset_ > other.start_offset_) {
      throw std::runtime_error("Getting text range in reverse.");
    }
    return TextRange(contents_, start_offset_, other.start_offset_);
  }

  bool AtStart() const { return start_offset_ == 0; }

 private:
  TextRange(std::shared_ptr<TextContents> contents, std::size_t start_offset,
            std::size_t end_offset)
      : contents_(contents),
        start_offset_(start_offset),
        end_offset_(end_offset) {}

  std::shared_ptr<TextContents> contents_;
  std::size_t start_offset_;
  std::size_t end_offset_;
};

}  // namespace text

#endif