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

  virtual LineOffset lineStartOffset() = 0;

  // Reads the next line from the input source.
  //
  // If true, the line is stored in inputPtr;
  virtual bool ReadNextLine() = 0;

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

  LineOffset lineStartOffset() override { return this->lineStart; }
  bool ReadNextLine() override;

  FILE* file;
  LineOffset lineStart;

 private:
  std::string curr_line_;
};

struct InputString : InputSource {
  InputString();
  InputString(std::string_view str);

  InputString& operator=(InputString&);

  LineOffset lineStartOffset() override { return 0; }
  bool ReadNextLine() override;

 private:
  std::string line_;
  bool wasRead_;
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

  bool CloseInputSource();

  bool IsDone();
  std::string_view GetRemainingLine();
  void SetRemainingLine(std::string_view line);

 private:
  std::vector<std::filesystem::path> includePath_;
};

extern InputState gInputState;

#endif
