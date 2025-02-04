//	input.hpp		sc
//		definitions for input source structure

#ifndef INPUT_HPP
#define INPUT_HPP

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using LineOffset = int;

struct InputSource {
  InputSource();
  InputSource(std::filesystem::path fileName, int lineNum = 0);
  virtual ~InputSource() {}

  InputSource& operator=(InputSource&);

  virtual bool endInputLine() = 0;
  virtual LineOffset lineStartOffset() = 0;

  virtual bool ReadNextLine(std::string* line) = 0;

  std::filesystem::path fileName;
  int lineNum;
  std::string_view inputPtr;

 private:
  friend class InputState;
  std::shared_ptr<InputSource> next_;
};

struct InputFile : InputSource {
  InputFile(FILE*, std::filesystem::path name);
  ~InputFile();

  bool endInputLine() override;
  LineOffset lineStartOffset() override { return this->lineStart; }
  bool ReadNextLine(std::string* line) override;

  FILE* file;
  LineOffset lineStart;
};

struct InputString : InputSource {
  InputString();
  InputString(std::string_view str);

  InputString& operator=(InputString&);

  bool endInputLine() override;
  LineOffset lineStartOffset() override { return 0; }
  bool ReadNextLine(std::string* line) override;

 private:
  std::optional<std::string> line_;
};

class InputState {
 public:
  // The name of the currently parsing source file.
  std::filesystem::path curFile;
  int curLine;
  // The current base source file, independent of current input stack.
  std::shared_ptr<InputSource> curSourceFile;
  std::shared_ptr<InputSource> inputSource;
  std::shared_ptr<InputSource> theFile;

  bool GetNewInputLine();
  void SetStringInput(std::string_view);

  void SetIncludePath(std::vector<std::string> const& extra_paths);
  void OpenFileAsInput(std::filesystem::path const& fileName, bool required);

  void SetInputToCurrentLine();
  void RestoreInput();

  bool CloseInputSource();

 private:
  std::vector<std::filesystem::path> includePath_;
  std::shared_ptr<InputSource> saveIs_;
  std::shared_ptr<InputString> curLineInput_;
};

extern InputState gInputState;

#endif
