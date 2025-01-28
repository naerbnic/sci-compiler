//	input.hpp		sc
//		definitions for input source structure

#ifndef INPUT_HPP
#define INPUT_HPP

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sc.hpp"

using LineOffset = int;

struct InputSource {
  InputSource();
  InputSource(std::string_view fileName, int lineNum = 0);
  virtual ~InputSource() {}

  InputSource& operator=(InputSource&);

  virtual bool incrementPastNewLine(std::string_view&) = 0;
  virtual bool endInputLine() = 0;
  virtual LineOffset lineStartOffset() = 0;

  std::shared_ptr<InputSource> next;
  std::string fileName;
  int lineNum;
  std::string_view inputPtr;
};

struct InputFile : InputSource {
  InputFile(FILE*, std::string_view name);
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
std::shared_ptr<InputSource> OpenFileAsInput(std::string_view, bool);
void SetIncludePath(std::vector<std::string> const& extra_paths);
void SetStringInput(std::string_view);
void SetInputToCurrentLine();
void RestoreInput();

void SetTokenStart();
void SetParseStart();
LineOffset GetParseStart();
LineOffset GetParsePos();
LineOffset GetTokenEnd();
void SetTokenEnd();

struct StrList;

extern std::string curFile;
extern int curLine;
extern std::shared_ptr<InputSource> curSourceFile;
extern std::vector<std::string> includePath;
extern std::shared_ptr<InputSource> is;
extern std::shared_ptr<InputSource> theFile;

#endif
