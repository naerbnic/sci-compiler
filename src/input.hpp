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
  // Sets the include path that will be used on include lookups.
  //
  // The paths will be searched in the order provided.
  void SetIncludePath(std::vector<std::string> const& extra_paths);

  // Retrieves the next input line, eliminating the remainder of the
  // line provided by GetRemainingLine().
  //
  // Returns false if there are no remaining lines left. IsDone() will
  // return true after false is returned here.
  bool GetNewInputLine();

  // Pushes the given string into the current line in the input.
  void SetStringInput(std::string_view);

  // Open a file that will be read. The top-level file filename and line will
  // refer to this file. This should only be called as the first call in a
  // given parse.
  void OpenTopLevelFile(std::filesystem::path const& fileName, bool required);

  // Pushes the given file on top of the input stack. The remaining text for
  // the current file on the top of the stack will be preserved until the
  // given file is exhausted.
  void OpenFileAsInput(std::filesystem::path const& fileName, bool required);

  // Gets the name of the current file that is being parsed. This may be a
  // file included from another file.
  std::string GetCurrFileName();

  // Gets the name of the top-level file this parse started with.
  std::string GetTopLevelFileName();

  // Gets the line within the current file.
  int GetCurrLineNum();

  // Gets the line with the top-level file.
  int GetTopLevelLineNum();

  // Returns true if we have exhausted the contents of this input.
  bool IsDone();
  // Gets the remaining portion of current line.
  std::string_view GetRemainingLine();

  // Sets the remaining portion of the current line. This must be a
  // substring of the string view returned from GetRemainingLine();
  void SetRemainingLine(std::string_view line);

 private:
  // Pops the current reading file from the input stack.
  bool CloseInputSource();
  // The current base source file, independent of current input stack.
  std::shared_ptr<InputSource> inputSource;
  std::vector<std::filesystem::path> includePath_;
  std::shared_ptr<InputSource> curSourceFile;
};

extern InputState gInputState;

#endif
