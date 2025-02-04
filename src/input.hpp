//	input.hpp		sc
//		definitions for input source structure

#ifndef INPUT_HPP
#define INPUT_HPP

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using LineOffset = int;

struct InputSource {
  InputSource();
  InputSource(std::filesystem::path fileName, int lineNum = 0);
  virtual ~InputSource() {}

  InputSource& operator=(InputSource&);

  virtual bool incrementPastNewLine(std::string_view&) = 0;
  virtual bool endInputLine() = 0;
  virtual LineOffset lineStartOffset() = 0;

  std::shared_ptr<InputSource> next;
  std::filesystem::path fileName;
  int lineNum;
  std::string_view inputPtr;
};

struct InputFile : InputSource {
  InputFile(FILE*, std::filesystem::path name);
  ~InputFile();

  bool incrementPastNewLine(std::string_view&) override;
  bool endInputLine() override;
  LineOffset lineStartOffset() override { return this->lineStart; }

  FILE* file;
  LineOffset lineStart;
};

struct InputString : InputSource {
  InputString();
  InputString(std::string_view str);

  InputString& operator=(InputString&);

  bool incrementPastNewLine(std::string_view&) override;
  bool endInputLine() override;
  LineOffset lineStartOffset() override { return 0; }
};

bool CloseInputSource();
bool GetNewInputLine();
std::shared_ptr<InputSource> OpenFileAsInput(
    std::filesystem::path const& fileName, bool);
void SetIncludePath(std::vector<std::string> const& extra_paths);
void SetStringInput(std::string_view);
void SetInputToCurrentLine();
void RestoreInput();

struct StrList;

extern std::filesystem::path gCurFile;
extern int gCurLine;
extern std::shared_ptr<InputSource> gCurSourceFile;
extern std::vector<std::filesystem::path> gIncludePath;
extern std::shared_ptr<InputSource> gInputSource;
extern std::shared_ptr<InputSource> gTheFile;

#endif
