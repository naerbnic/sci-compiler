//	input.cpp	sc
// 	input routines

#include "input.hpp"

#include <stdlib.h>

#include <memory>

#include "error.hpp"
#include "fileio.hpp"
#include "jeff.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "string.hpp"
#include "text.hpp"
#include "token.hpp"

std::string curFile;
int curLine;
std::shared_ptr<InputSource> curSourceFile;
StrList* includePath;
std::shared_ptr<InputSource> is;
std::shared_ptr<InputSource> theFile;

static char inputLine[512];

static void SetInputSource(const std::shared_ptr<InputSource>& nis);

InputSource::InputSource() : fileName(0), lineNum(0) {
  tokSym.setStr(nullptr);
  curLine = lineNum;
}

InputSource::InputSource(std::string_view fileName, int lineNum)
    :

      fileName(fileName),
      lineNum(lineNum) {
  tokSym.setStr(symStr);
  curLine = lineNum;
}

InputSource& InputSource::operator=(InputSource& s) {
  fileName = s.fileName;
  lineNum = s.lineNum;
  curLine = lineNum;
  return *this;
}

InputFile::InputFile(FILE* fp, const char* name)
    : InputSource(newStr(name)), file(fp) {
  curFile = fileName;
}

InputFile::~InputFile() { fclose(file); }

bool InputFile::incrementPastNewLine(const char*& ip) {
  if (GetNewLine()) {
    ip = is->ptr;
    return True;
  }
  return False;
}

bool InputFile::endInputLine() { return GetNewLine(); }

InputString::InputString(const char* str) : InputSource(curFile, curLine) {
  ptr = str;
}

InputString::InputString() { ptr = 0; }

InputString& InputString::operator=(InputString& s) {
  InputSource::operator=(s);
  ptr = s.ptr;
  return *this;
}

bool InputString::endInputLine() { return CloseInputSource(); }

bool InputString::incrementPastNewLine(const char*& ip) {
  ++ip;
  return True;
}

std::shared_ptr<InputSource> OpenFileAsInput(strptr fileName, bool required) {
  FILE* file;
  std::shared_ptr<InputSource> theFile;
  char newName[_MAX_PATH + 1];
  StrList* ip;

  // Try to open the file.  If we can't, try opening it in each of
  // the directories in the include path.
  ip = includePath;
  *newName = '\0';
  for (file = fopen(fileName, "r"); !file && ip; ip = ip->next) {
    MakeName(newName, ip->str, fileName, fileName);
    file = fopen(newName, "r");
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

#if defined(PLAYGRAMMER)
  theFile->fullFileName = newStr(newName);
#endif
  SetInputSource(theFile);
  GetNewLine();

  return theFile;
}

bool CloseInputSource() {
  // Close the current input source.  If the source is a file, this involves
  // closing the file.  Remove the source from the chain and free its memory.

  std::shared_ptr<InputSource> ois;

  if ((ois = is)) {
    std::shared_ptr<InputSource> next = is->next;
    is = next;
  }

  if (is) {
    curFile = is->fileName;
    curLine = is->lineNum;
  }

  return (bool)is;
}

void SetStringInput(strptr str) {
  SetInputSource(std::make_shared<InputString>(str));
}

bool GetNewInputLine() {
  // Read a new line in from the current input file.  If we're at end of
  // file, close the file, shifting input to the next source in the queue.

  while (is) {
#if defined(PLAYGRAMMER)
    fgetpos(is->file, &is->lineStart);
#endif
    if ((is->ptr =
             fgets(inputLine, sizeof inputLine, ((InputFile*)is.get())->file)))
      break;
    CloseInputSource();
  }

  if (is) {
    ++is->lineNum;
    ++curLine;
  }

  return (bool)is;
}

void SetIncludePath() {
  strptr t;
  char* p;
  char path[128];
  StrList* sn;
  StrList* last;

  if (!(t = getenv("SINCLUDE"))) return;

  // Successively copy each element of the path into 'path',
  // and add it to the includePath chain.
  for (p = path, last = 0; *t; p = path) {
    // Scan to end of environment or a ';'
    for (; *t && *t != ';'; ++t, ++p) *p = (char)(*t == '\\' ? '/' : *t);

    // Make sure that the path terminates with either a '/' or ':'
    if (p[-1] != '/' && p[-1] != ':') *p++ = '/';
    *p = '\0';

    // Point past a ';' (but not the end of the environment).
    if (*t) ++t;

    // Now allocate a node to keep this path on in the includePath chain
    // and link it into the list.
    sn = new StrList;
    if (!last)
      last = includePath = sn;
    else {
      last->next = sn;
      last = sn;
    }

    // Save the pathname and hang it off the node.
    sn->str = newStr(path);
  }
}

void FreeIncludePath() {
  // Free the memory alloted to the include path.

  StrList* sn;
  StrList* tmp;

  for (sn = includePath; sn;) {
    tmp = sn->next;
    delete[] sn->str;
    delete sn;
    sn = tmp;
  }
  includePath = 0;
}

static std::shared_ptr<InputSource> saveIs;
static std::shared_ptr<InputString> curLineInput;

void SetInputToCurrentLine() {
  //	set the current input line as the input source

  saveIs = is;
  curLineInput = std::make_shared<InputString>(inputLine);
  is = curLineInput;
}

void RestoreInput() {
  if (saveIs) {
    is = saveIs;
    saveIs = 0;
  }
}

static void SetInputSource(const std::shared_ptr<InputSource>& nis) {
  // Link a new input source (pointed to by 'nis') in at the head of the input
  // source queue.

  nis->next = is;
  is = nis;
}

////////////////////////////////////////////////////////////////////////////

static fpos_t startToken;
static fpos_t endToken;
static fpos_t startParse;

void SetTokenStart() { startToken = GetParsePos(); }

void SetParseStart() { startParse = startToken; }

fpos_t GetParseStart() { return startParse; }

fpos_t GetParsePos() {
  return static_cast<InputFile*>(is.get())->lineStart +
         (long)(is->ptr - inputLine);
}

fpos_t GetTokenEnd() { return endToken; }

void SetTokenEnd() { endToken = GetParsePos() - 1; }
