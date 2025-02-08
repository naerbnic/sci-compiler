#include "util/io/buffer.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_format.h"

namespace io {
namespace {

std::string::size_type FindNewline(const std::string& str) {
  auto newline = str.find_first_of("\r\n");
  if (newline == std::string::npos) {
    return std::string::npos;
  }
  if (str[newline] == '\n') {
    return newline + 1;
  }

  if (str[newline] == '\r') {
    if (newline + 1 < str.size() && str[newline + 1] == '\n') {
      return newline + 2;
    }
    return newline + 1;
  }

  throw std::runtime_error("Unexpected character in FindNewline");
}

class ReadBufferBase : public ReadBuffer {
 public:
  void LoadToNextNewline() {
    auto newline = FindNewline(buffer_);
    while (newline == std::string::npos) {
      if (at_end_) {
        currLineLen_ = buffer_.size();
        return;
      }
      if (!ReadToBuffer(buffer_)) {
        currLineLen_ = buffer_.size();
        at_end_ = true;
        return;
      }

      newline = FindNewline(buffer_);
    }

    currLineLen_ = newline;
  }

  int GetLineIndex() const override { return lineIndex_; }

  std::string_view CurrLine() const override {
    return std::string_view(buffer_).substr(0, currLineLen_);
  }

  void AdvanceLine() override {
    if (!IsAtEnd()) {
      buffer_.erase(0, currLineLen_);
      ++lineIndex_;
      LoadToNextNewline();
    }
  }

  bool IsAtEnd() const override { return buffer_.empty() && at_end_; }

 protected:
  virtual bool ReadToBuffer(std::string& buffer) = 0;

 private:
  bool at_end_ = false;
  std::string buffer_;
  // Length of the current line in the buffer, starting from the
  // beginning of the buffer.
  std::size_t currLineLen_ = 0;
  int lineIndex_ = 0;
};

class FileReadBuffer : public ReadBufferBase {
 public:
  FileReadBuffer(FILE* file) : file_(file) {}
  ~FileReadBuffer() override { fclose(file_); }

 protected:
  bool ReadToBuffer(std::string& buffer) override {
    std::array<char, 1024> buf;
    auto result = fread(buf.data(), sizeof(char), buf.size(), file_);
    if (result < 0) {
      auto currErr = errno;
      throw std::runtime_error(absl::StrFormat(
          "Error reading file. errno: %d, %s", currErr, strerror(currErr)));
    }
    if (result == 0) {
      return false;
    }

    buffer.append(buf.data(), result);
    return true;
  }

 private:
  FILE* file_;
};

class StringReadBuffer : public ReadBufferBase {
 public:
  StringReadBuffer(std::string str) : str_(std::move(str)) {}

 protected:
  bool ReadToBuffer(std::string& buffer) override {
    if (str_.empty()) {
      return false;
    }
    buffer.append(std::move(str_));
    str_.clear();
    return true;
  }

 private:
  std::string str_;
};

}  // namespace

std::unique_ptr<ReadBuffer> ReadBuffer::FromFile(FILE* file) {
  auto buffer = std::make_unique<FileReadBuffer>(file);
  buffer->LoadToNextNewline();
  return buffer;
}

std::unique_ptr<ReadBuffer> ReadBuffer::FromString(std::string str) {
  auto buffer = std::make_unique<StringReadBuffer>(str);
  buffer->LoadToNextNewline();
  return buffer;
}

}  // namespace io
