#include "scic/text/text_range.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace text {

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

TextContents::TextContents(std::string contents)
    : TextContents("<string>", std::move(contents)) {}

TextContents::TextContents(std::string filename, std::string contents)
    : filename_(std::make_shared<std::string>(std::move(filename))),
      contents_(std::move(contents)) {
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

TextRange TextRange::OfString(std::string contents) {
  auto length = contents.size();
  return TextRange(
      std::make_shared<TextContents>("<string>", std::move(contents)), 0,
      length);
}

TextRange TextRange::WithFilename(std::string filename, std::string contents) {
  auto length = contents.size();
  return TextRange(
      std::make_shared<TextContents>(std::move(filename), std::move(contents)),
      0, length);
}

TextRange::~TextRange() = default;

}  // namespace tokens