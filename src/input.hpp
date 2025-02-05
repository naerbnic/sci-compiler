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
  InputSource() = default;
  virtual ~InputSource() = default;

  // Reads the next line from the input source.
  //
  // If true, the line is stored in inputPtr;
  virtual bool ReadNextLine(std::string*) = 0;
};

struct InputFile : InputSource {
  InputFile(FILE* fp);
  ~InputFile();

  bool ReadNextLine(std::string* output) override;

 private:
  FILE* file_;
};

struct InputString : InputSource {
  InputString(std::string_view str);

  bool ReadNextLine(std::string* output) override;

 private:
  std::optional<std::string> line_;
};

class InputState {
 public:
  InputState();
  ~InputState();
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
  struct InputSourceState;

  void PushInput(std::filesystem::path const& fileName, int lineNum,
                 std::unique_ptr<InputSource> input);
  // The current base source file, independent of current input stack.
  std::vector<std::filesystem::path> includePath_;

  std::vector<std::unique_ptr<InputSourceState>> inputStack_;
};

extern InputState gInputState;

#endif
