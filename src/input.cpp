//	input.cpp	sc
// 	input routines

#include "input.hpp"

#include <stdlib.h>

#include <filesystem>
#include <memory>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "error.hpp"

InputState gInputState;

static FILE* FOpen(std::filesystem::path const& path, const char* mode) {
  return fopen(path.string().c_str(), mode);
}

InputSource::InputSource() : lineNum(0), fileName("") {}

InputSource::InputSource(std::filesystem::path fileName, int lineNum)
    : lineNum(lineNum), fileName(fileName) {}

InputSource& InputSource::operator=(InputSource& s) {
  fileName = s.fileName;
  lineNum = s.lineNum;
  return *this;
}

InputFile::InputFile(FILE* fp, std::filesystem::path name)
    : InputSource(name), file(fp) {}

InputFile::~InputFile() { fclose(file); }

bool InputFile::ReadNextLine() {
  std::array<char, 512> inputLine;

  if (!fgets(inputLine.data(), inputLine.size(), file)) {
    if (feof(file)) {
      inputPtr = "";
      return false;
    }
    int error = ferror(file);
    throw std::runtime_error(
        absl::StrFormat("I/O error reading file %s: %d", fileName, error));
  }

  curr_line_.clear();
  curr_line_.append(inputLine.data());

  inputPtr = curr_line_;

  return true;
}

InputString::InputString() : line_(), wasRead_(true) { inputPtr = ""; }

InputString::InputString(std::filesystem::path fileName, int lineNum,
                         std::string_view str)
    : InputSource(std::move(fileName), lineNum),
      line_(std::string(str)),
      wasRead_(false) {
  inputPtr = "";
}

InputString& InputString::operator=(InputString& s) {
  InputSource::operator=(s);
  inputPtr = s.inputPtr;
  return *this;
}

bool InputString::ReadNextLine() {
  if (wasRead_) {
    return false;
  }
  inputPtr = line_;
  wasRead_ = true;
  return true;
}

// -------------
// InputState

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
    if (inputStack_.back()->ReadNextLine()) {
      break;
    }
    CloseInputSource();
  }

  if (!inputStack_.empty()) {
    ++inputStack_.back()->lineNum;
  }

  return !inputStack_.empty();
}

void InputState::SetStringInput(std::string_view str) {
  auto nis =
      std::make_unique<InputString>(GetCurrFileName(), GetCurrLineNum(), str);
  inputStack_.push_back(std::move(nis));
}

void InputState::SetIncludePath(std::vector<std::string> const& extra_paths) {
  const char* t = getenv("SINCLUDE");

  if (t) {
    // Successively copy each element of the path into 'path',
    // and add it to the includePath_ chain.
    for (auto path : absl::StrSplit(t, ';')) {
      // Now allocate a node to keep this path on in the includePath_
      // chain and link it into the list.
      includePath_.emplace_back(path);
    }
  }

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

  inputStack_.push_back(std::make_unique<InputFile>(file, fileName));
}

bool InputState::CloseInputSource() {
  // Close the current input source.  If the source is a file, this involves
  // closing the file.  Remove the source from the chain and free its memory.

  if (inputStack_.empty()) {
    return false;
  }
  inputStack_.pop_back();

  return !inputStack_.empty();
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