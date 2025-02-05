//	input.cpp	sc
// 	input routines

#include "input.hpp"

#include <stdlib.h>

#include <filesystem>
#include <memory>

#include "absl/strings/str_format.h"
#include "error.hpp"

InputState gInputState;

namespace {

FILE* FOpen(std::filesystem::path const& path, const char* mode) {
  return fopen(path.string().c_str(), mode);
}

struct InputFile : InputSource {
  InputFile(FILE* fp) : file_(fp) {}
  ~InputFile() { fclose(file_); }

  bool ReadNextLine(std::string* output) {
    std::array<char, 512> inputLine;

    if (!fgets(inputLine.data(), inputLine.size(), file_)) {
      if (feof(file_)) {
        return false;
      }
      throw std::runtime_error("I/O error reading file");
    }

    output->append(inputLine.data());

    return true;
  }

 private:
  FILE* file_;
};

struct InputString : InputSource {
  InputString(std::string_view str) : line_(std::string(str)) {}

  bool ReadNextLine(std::string* output) {
    if (!line_) {
      return false;
    }
    *output = std::move(line_).value();
    line_ = std::nullopt;
    return true;
  }

 private:
  std::optional<std::string> line_;
};

}  // namespace

// -------------
// InputState

struct InputState::InputSourceState {
  std::filesystem::path fileName;
  int lineNum;
  std::string currLine;
  std::string_view inputPtr;
  std::unique_ptr<InputSource> inputSource;
};

// We need to define these here because InputSourceState has to be defined
// before the destructor is defined, so the std::unique_ptrs know how to
// destroy themselves.
InputState::InputState() = default;
InputState::~InputState() = default;

bool InputState::GetNewInputLine() {
  // Read a new line in from the current input file.  If we're at end of
  // file, close the file, shifting input to the next source in the queue.

  // This now observes if inputSources have an empty input. We only go back
  // far enough to find a source with input.
  //
  // To ensure we don't just wait at the current source forever, we
  // set the current input to empty.
  if (!inputStack_.empty()) {
    inputStack_.back()->inputPtr = "";
  }

  while (!inputStack_.empty() && inputStack_.back()->inputPtr.empty()) {
    auto& currInput = inputStack_.back();
    currInput->currLine.clear();
    if (currInput->inputSource->ReadNextLine(&currInput->currLine)) {
      currInput->inputPtr = currInput->currLine;
      break;
    }

    inputStack_.pop_back();
  }

  if (!inputStack_.empty()) {
    ++inputStack_.back()->lineNum;
  }

  return !inputStack_.empty();
}

void InputState::SetStringInput(std::string_view str) {
  PushInput(GetCurrFileName(), GetCurrLineNum(),
            std::make_unique<InputString>(str));
}

void InputState::SetIncludePath(std::vector<std::string> const& extra_paths) {
  std::ranges::copy(extra_paths, std::back_inserter(includePath_));
}

void InputState::OpenTopLevelFile(std::filesystem::path const& fileName,
                                  bool required) {
  if (!inputStack_.empty()) {
    Warning("Top level file specified with other input sources open");
  }

  OpenFileAsInput(fileName, required);
}

void InputState::OpenFileAsInput(std::filesystem::path const& fileName,
                                 bool required) {
  // Try to open the file.  If we can't, try opening it in each of
  // the directories in the include path.
  FILE* file = FOpen(fileName, "r");
  if (!file && fileName.is_relative()) {
    for (auto const& path : includePath_) {
      file = FOpen(path / fileName, "r");
      if (file) break;
    }
  }

  if (!file) {
    if (required) Panic("Can't open \"%s\"", fileName);
  }

  PushInput(fileName, 1, std::make_unique<InputFile>(file));
}

std::string InputState::GetCurrFileName() {
  if (inputStack_.empty()) {
    return "<unknown>";
  }
  return inputStack_.back()->fileName.string();
}

std::string InputState::GetTopLevelFileName() {
  if (inputStack_.empty()) {
    return "<unknown>";
  }

  return inputStack_.front()->fileName.string();
}

int InputState::GetCurrLineNum() {
  if (inputStack_.empty()) {
    return 0;
  }

  return inputStack_.back()->lineNum;
}

int InputState::GetTopLevelLineNum() {
  if (inputStack_.empty()) {
    return 0;
  }

  return inputStack_.front()->lineNum;
}

bool InputState::IsDone() { return inputStack_.empty(); }

std::string_view InputState::GetRemainingLine() {
  if (inputStack_.empty()) {
    return "";
  }
  return inputStack_.back()->inputPtr;
}

void InputState::SetRemainingLine(std::string_view str) {
  if (inputStack_.empty()) {
    throw std::runtime_error(
        "Unexpeted setting of remaining line after closing");
  }

  auto& currInput = inputStack_.back();

  if (!str.empty()) {
    // Enforce that this must be a part of the current line's
    // memory range.
    auto currInputStr = currInput->inputPtr;
    auto const* low_ptr = &currInputStr[0];
    auto const* high_ptr = &currInputStr[currInputStr.size()];
    auto const* str_start = &str[0];
    auto const* str_end = &str[str.size()];
    bool start_in_range = str_start >= low_ptr && str_start <= high_ptr;
    bool end_in_range = str_end >= low_ptr && str_end <= high_ptr;
    if (!start_in_range || !end_in_range) {
      throw new std::runtime_error(
          "Updated line out of bounds of original line");
    }
  }

  currInput->inputPtr = str;
}

void InputState::PushInput(std::filesystem::path const& fileName, int lineNum,
                           std::unique_ptr<InputSource> input) {
  inputStack_.push_back(std::make_unique<InputSourceState>(InputSourceState{
      .fileName = fileName,
      .lineNum = lineNum,
      .currLine = "",
      .inputPtr = "",
      .inputSource = std::move(input),
  }));
}