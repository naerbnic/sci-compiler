//	input.cpp	sc
// 	input routines

#include "input.hpp"

#include <stdlib.h>

#include <filesystem>
#include <memory>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "error.hpp"
#include "token.hpp"

std::filesystem::path gCurFile;
int gCurLine;
std::shared_ptr<InputSource> gCurSourceFile;
std::vector<std::filesystem::path> gIncludePath;
std::shared_ptr<InputSource> gInputSource;
std::shared_ptr<InputSource> gTheFile;

static char inputLine[512];

static void SetInputSource(const std::shared_ptr<InputSource>& nis);

static FILE* FOpen(std::filesystem::path const& path, const char* mode) {
  return fopen(path.string().c_str(), mode);
}

InputSource::InputSource() : fileName(0), lineNum(0) {
  gTokSym.setStr("");
  gCurLine = lineNum;
}

InputSource::InputSource(std::filesystem::path fileName, int lineNum)
    : fileName(fileName), lineNum(lineNum) {
  gTokSym.setStr(gSymStr);
  gCurLine = lineNum;
}

InputSource& InputSource::operator=(InputSource& s) {
  fileName = s.fileName;
  lineNum = s.lineNum;
  gCurLine = lineNum;
  return *this;
}

InputFile::InputFile(FILE* fp, std::filesystem::path name)
    : InputSource(name), file(fp) {
  gCurFile = fileName;
}

InputFile::~InputFile() { fclose(file); }

bool InputFile::incrementPastNewLine(std::string_view& ip) {
  if (GetNewLine()) {
    ip = gInputSource->inputPtr;
    return true;
  }
  return false;
}

bool InputFile::endInputLine() { return GetNewLine(); }

InputString::InputString(std::string_view str) : InputSource(gCurFile, gCurLine) {
  inputPtr = str;
}

InputString::InputString() { inputPtr = ""; }

InputString& InputString::operator=(InputString& s) {
  InputSource::operator=(s);
  inputPtr = s.inputPtr;
  return *this;
}

bool InputString::endInputLine() { return CloseInputSource(); }

bool InputString::incrementPastNewLine(std::string_view& ip) {
  ip = ip.substr(1);
  return true;
}

std::shared_ptr<InputSource> OpenFileAsInput(
    std::filesystem::path const& fileName, bool required) {
  std::shared_ptr<InputSource> theFile;

  // Try to open the file.  If we can't, try opening it in each of
  // the directories in the include path.
  FILE* file = FOpen(fileName, "r");
  if (!file && fileName.is_relative()) {
    for (auto const& path : gIncludePath) {
      file = FOpen(path / fileName, "r");
    }
  }

  if (!file) {
    if (required)
      Panic("Can't open \"%s\"", fileName);
    else
      return 0;
  }

  // SLN - following code serves no purpose, removed
  //	if (!*newName)
  //		strcpy(newName, fileName);
  //	fullyQualify(newName);

  theFile = std::make_shared<InputFile>(file, fileName);

  SetInputSource(theFile);
  GetNewLine();

  return theFile;
}

bool CloseInputSource() {
  // Close the current input source.  If the source is a file, this involves
  // closing the file.  Remove the source from the chain and free its memory.

  std::shared_ptr<InputSource> ois;

  if ((ois = gInputSource)) {
    std::shared_ptr<InputSource> next = gInputSource->next;
    gInputSource = next;
  }

  if (gInputSource) {
    gCurFile = gInputSource->fileName;
    gCurLine = gInputSource->lineNum;
  }

  return (bool)gInputSource;
}

void SetStringInput(std::string_view str) {
  SetInputSource(std::make_shared<InputString>(str));
}

bool GetNewInputLine() {
  // Read a new line in from the current input file.  If we're at end of
  // file, close the file, shifting input to the next source in the queue.

  while (gInputSource) {
    if (fgets(inputLine, sizeof inputLine, ((InputFile*)gInputSource.get())->file)) {
      gInputSource->inputPtr = inputLine;
      break;
    }
    CloseInputSource();
  }

  if (gInputSource) {
    ++gInputSource->lineNum;
    ++gCurLine;
  }

  return (bool)gInputSource;
}

void SetIncludePath(std::vector<std::string> const& extra_paths) {
  const char* t = getenv("SINCLUDE");

  if (t) {
    // Successively copy each element of the path into 'path',
    // and add it to the gIncludePath chain.
    for (auto path : absl::StrSplit(t, ';')) {
      // Now allocate a node to keep this path on in the gIncludePath chain
      // and link it into the list.
      gIncludePath.emplace_back(path);
    }
  }

  std::ranges::copy(extra_paths, std::back_inserter(gIncludePath));
}

static std::shared_ptr<InputSource> saveIs;
static std::shared_ptr<InputString> curLineInput;

void SetInputToCurrentLine() {
  //	set the current input line as the input source

  saveIs = gInputSource;
  curLineInput = std::make_shared<InputString>(inputLine);
  gInputSource = curLineInput;
}

void RestoreInput() {
  if (saveIs) {
    gInputSource = saveIs;
    saveIs = 0;
  }
}

static void SetInputSource(const std::shared_ptr<InputSource>& nis) {
  // Link a new input source (pointed to by 'nis') in at the head of the input
  // source queue.

  nis->next = gInputSource;
  gInputSource = nis;
}
