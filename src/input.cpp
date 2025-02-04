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

InputSource::InputSource() : fileName(""), lineNum(0) {
  gInputState.curLine = lineNum;
}

InputSource::InputSource(std::filesystem::path fileName, int lineNum)
    : fileName(fileName), lineNum(lineNum) {
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

void InputState::OpenFileAsInput(std::filesystem::path const& fileName,
                                 bool required) {
  std::shared_ptr<InputSource> theFile;

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

  theFile = std::make_shared<InputFile>(file, fileName);

  theFile->next_ = inputSource;
  inputSource = theFile;
  this->theFile = theFile;
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