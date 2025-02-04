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

 protected:
  int lineNum;
  std::string_view inputPtr;
  std::filesystem::path fileName;

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
  InputString(std::filesystem::path fileName, int lineNum,
              std::string_view str);

  InputString& operator=(InputString&);

  LineOffset lineStartOffset() override { return 0; }
  bool ReadNextLine() override;

 private:
  std::string line_;
  bool wasRead_;
};

class InputState {
 public:
  // The current base source file, independent of current input stack.
  std::shared_ptr<InputSource> inputSource;

  bool GetNewInputLine();
  void SetStringInput(std::string_view);

  void SetIncludePath(std::vector<std::string> const& extra_paths);
  void OpenTopLevelFile(std::filesystem::path const& fileName, bool required);
  void OpenFileAsInput(std::filesystem::path const& fileName, bool required);

  bool CloseInputSource();

  std::string GetCurrFileName();
  std::string GetTopLevelFileName();
  int GetCurrLineNum();
  int GetTopLevelLineNum();
  bool IsDone();
  std::string_view GetRemainingLine();
  void SetRemainingLine(std::string_view line);

 private:
  std::vector<std::filesystem::path> includePath_;
  std::shared_ptr<InputSource> curSourceFile;
};

extern InputState gInputState;

#endif
