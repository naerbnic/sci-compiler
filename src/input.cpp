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

InputSource::InputSource() : lineNum(0), fileName("") {
  gInputState.curLine = lineNum;
}

InputSource::InputSource(std::filesystem::path fileName, int lineNum)
    : lineNum(lineNum), fileName(fileName) {
  gInputState.curLine = lineNum;
}

InputSource& InputSource::operator=(InputSource& s) {
  fileName = s.fileName;
  lineNum = s.lineNum;
  gInputState.curLine = lineNum;
  return *this;
}

InputFile::InputFile(FILE* fp, std::filesystem::path name)
    : InputSource(name), file(fp) {
  gInputState.curFile = fileName;
}

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

InputString::InputString(std::string_view str)
    : InputSource(gInputState.curFile, gInputState.curLine),
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
  if (inputSource) {
    inputSource->inputPtr = "";
  }

  while (inputSource && inputSource->inputPtr.empty()) {
    if (inputSource->ReadNextLine()) {
      break;
    }
    CloseInputSource();
  }

  if (inputSource) {
    ++inputSource->lineNum;
    ++curLine;
  }

  return (bool)inputSource;
}

void InputState::SetStringInput(std::string_view str) {
  auto nis = std::make_shared<InputString>(str);
  nis->next_ = inputSource;
  inputSource = nis;
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
  if (inputSource != nullptr) {
    Warning("Top level file specified with other input sources open");
  }

  OpenFileAsInput(fileName, required);
  curSourceFile = inputSource;
}

void InputState::OpenFileAsInput(std::filesystem::path const& fileName,
                                 bool required) {
  std::shared_ptr<InputSource> localFile;

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

  localFile = std::make_shared<InputFile>(file, fileName);

  localFile->next_ = inputSource;
  inputSource = localFile;
}

bool InputState::CloseInputSource() {
  // Close the current input source.  If the source is a file, this involves
  // closing the file.  Remove the source from the chain and free its memory.

  std::shared_ptr<InputSource> ois;

  if ((ois = inputSource)) {
    std::shared_ptr<InputSource> next = inputSource->next_;
    inputSource = next;
  }

  if (inputSource) {
    curFile = inputSource->fileName;
    curLine = inputSource->lineNum;
  }

  return (bool)inputSource;
}

std::string InputState::GetCurrFileName() {
  if (!inputSource) {
    return "<unknown>";
  }
  return inputSource->fileName.string();
}

std::string InputState::GetTopLevelFileName() {
  if (!curSourceFile) {
    return "<unknown>";
  }

  return curSourceFile->fileName.string();
}

int InputState::GetTopLevelLineNum() {
  if (!curSourceFile) {
    return 0;
  }

  return curSourceFile->lineNum;
}

bool InputState::IsDone() { return !inputSource; }

std::string_view InputState::GetRemainingLine() {
  if (!inputSource) {
    return "";
  }
  return inputSource->inputPtr;
}

void InputState::SetRemainingLine(std::string_view str) {
  if (!inputSource) {
    throw std::runtime_error(
        "Unexpeted setting of remaining line after closing");
  }

  if (!str.empty()) {
    // Enforce that this must be a part of the current line's
    // memory range.
    auto currInput = inputSource->inputPtr;
    auto const* low_ptr = &currInput[0];
    auto const* high_ptr = &currInput[currInput.size()];
    auto const* str_start = &str[0];
    auto const* str_end = &str[str.size()];
    bool start_in_range = str_start >= low_ptr || str_start <= high_ptr;
    bool end_in_range = str_end >= low_ptr || str_end <= high_ptr;
    if (!start_in_range || !end_in_range) {
      throw new std::runtime_error(
          "Updated line out of bounds of original line");
    }
  }

  inputSource->inputPtr = str;
}