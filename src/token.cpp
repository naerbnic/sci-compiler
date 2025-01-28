//	token.cpp		sc
// 	return the next token from the input

#include "token.hpp"

#include <stdlib.h>
#include <string.h>

#include <string_view>

#include "absl/strings/ascii.h"
#include "chartype.hpp"
#include "error.hpp"
#include "input.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "symtbl.hpp"

#define ALT_QUOTE '{'

int nestedCondCompile;
std::string symStr;
TokenSlot tokSym;

static constexpr std::string_view binDigits = "01";
static constexpr std::string_view decDigits = "0123456789";
static bool haveUnGet;
static constexpr std::string_view hexDigits = "0123456789abcdef";
static TokenSlot lastTok;

//	preprocessor tokens
enum pt {
  PT_NONE,
  PT_IF,
  PT_IFDEF,
  PT_IFNDEF,
  PT_ELIF,
  PT_ELIFDEF,
  PT_ELIFNDEF,
  PT_ELSE,
  PT_ENDIF
};

static pt GetPreprocessorToken();
static void ReadKey(strptr ip);
static void ReadString(strptr ip);
static void ReadNumber(strptr ip);

void GetToken() {
  // Get a token and bitch if one is not available.
  if (!NewToken()) EarlyEnd();

  // Save the token type and value returned, in case we want to 'unget'
  // the token after changing them.
  lastTok = tokSym;
}

bool NewToken() {
  // Get a new token and handle replacement if the token is a define or enum.

  Symbol* theSym;

  if (NextToken()) {
    if (symType == S_IDENT && (theSym = syms.lookup(symStr)) &&
        theSym->type == S_DEFINE) {
      SetStringInput(theSym->str());
      NewToken();
    }
    return True;
  }
  return False;
}

void UnGetTok() { haveUnGet = True; }

void GetRest(bool error) {
  // Copy the rest of the parenthesized expression into 'symStr'.
  //	If just seeking until next closing paren, don't display further
  //	messages.

  strptr ip;
  int pLevel;
  bool truncate;

  if (error && !is) return;

  ip = is->ptr;
  symStr.clear();
  pLevel = 0;
  truncate = False;

  while (1) {
    switch (*ip) {
      case '(':
        ++pLevel;
        break;

      case ')':
        if (pLevel > 0)
          --pLevel;
        else {
          symType = S_STRING;
          is->ptr = ip;
          SetTokenEnd();
          return;
        }
        break;

      case '\n':
        if (!is->incrementPastNewLine(ip)) EarlyEnd();
        continue;

      case '\0':
        CloseInputSource();
        if (!is) {
          if (!error) EarlyEnd();
          return;
        }
        ip = is->ptr;
        continue;
    }

    if (!truncate) symStr.push_back(*ip);
    ++ip;

    if (symStr.length() >= MaxTokenLen && !truncate) {
      if (!error) Warning("Define too long.  Truncated.");
      truncate = True;
    }
  }
}

bool NextToken() {
  // Return the next token.  If we're at the end of the current input source,
  // close it and get input from the previous source in the queue.

  strptr ip;  // pointer to input line
  ubyte c;    // the character

  if (haveUnGet) {
    haveUnGet = False;
    tokSym = lastTok;
    return True;
  }

  if (!is) {
    symType = S_END;
    return False;
  }

  // Get pointer to input in a register
  ip = is->ptr;

  // Scan to the start of the next token.
  while (IsSep(*ip)) {
    // Eat any whitespace.
    while (*ip == '\t' || *ip == ' ') ++ip;

    // If we hit the start of a comment, skip it.
    if (*ip == ';')
      while (*ip != '\n' && *ip != '\0') ++ip;

    if (*ip == '\0' || *ip == '\n') {
      if (is->endInputLine())
        ip = is->ptr;
      else {
        symType = S_END;
        return False;
      }
    }
  }

  // At this point, we are at the beginning of a valid token.
  // The token type can be determined by examining the first character,
  // except in the case of '-', which could be either an operator
  // or a unary minus which starts a number.  The minus operator
  // can be distinguished in this case because the next character will
  // be a digit.
  // If we encounter a '\0' in the scan, it does not terminate the
  // character, but simply causes us to move to another input source.

  SetTokenStart();

  c = *ip;
  symStr.clear();

  if (IsTok(c)) {
    symStr.push_back(c);
    symType = (sym_t)c;
    is->ptr = ++ip;
    SetTokenEnd();
    return True;
  }

  if (c == '`') {
    // A character constant.
    ReadKey(++ip);
    return True;
  }

  if (c == '"' || c == ALT_QUOTE) {
    ReadString(ip);
    return True;
  }

  if (IsDigit(c) || (c == '-' && IsDigit(*(ip + 1)))) {
    symType = S_NUM;
    ReadNumber(ip);
    return True;
  }

  symType = S_IDENT;
  while (!IsTerm(c)) {
    ++ip;
    if (c == ':') {
      // This is a selector literal (e.g. 'foo:'). Only include the part
      // before the quote, but mark the sym type.
      symType = S_SELECT_LIT;
      break;
    }
    if (IsIncl(c)) break;
    symStr.push_back(c);
    c = *ip;
  }
  is->ptr = ip;
  SetTokenEnd();

  return True;
}

bool GetNewLine() {
  //	Read a new line from the input source, skipping over lines as
  //	indicated by preprocessor directives (#if, #else, etc.)
  //
  //	This is complicated by the fact that we don't want to use normal
  //	tokenizing, since we don't want to tokenize source that will never be
  //	compiled, as that might contain token errors such as a too long string.
  //
  //	Hence, we temporarily limit the input source to the current line (as
  //	preprocessor directives must all be on a single line), and call a
  //	special preprocessor token routine, that just knows about its own
  //	tokens.  If successful, we can then use the "normal" GetNumber()
  //	tokenizing to evaluate the argument of the directive.
  //
  //	This function is set up as a state machine with three states:
  //	compiling, notCompiling and gettingEndif

  int level;

compiling:
  while (1) {
    //	if input was limited to the current line before, unlimit it
    RestoreInput();
    if (!GetNewInputLine()) return False;
    SetInputToCurrentLine();

    switch (GetPreprocessorToken()) {
      case PT_IF:
        if (!GetNumber("Constant expression")) setSymVal(0);
        ++nestedCondCompile;
        if (!symVal) goto notCompiling;
        break;

      case PT_IFDEF:
        ++nestedCondCompile;
        if (!GetDefineSymbol()) goto notCompiling;
        break;

      case PT_IFNDEF:
        ++nestedCondCompile;
        if (GetDefineSymbol()) goto notCompiling;
        break;

      case PT_ELIF:
        if (!nestedCondCompile) Error("#elif without corresponding #if");
        goto gettingEndif;

      case PT_ELIFDEF:
        if (!nestedCondCompile) Error("#elifdef without corresponding #if");
        goto gettingEndif;

      case PT_ELIFNDEF:
        if (!nestedCondCompile) Error("#elifndef without corresponding #if");
        goto gettingEndif;

      case PT_ELSE:
        if (!nestedCondCompile) Error("#else without corresponding #if");
        goto gettingEndif;

      case PT_ENDIF:
        if (!nestedCondCompile)
          Error("#endif without corresponding #if");
        else
          --nestedCondCompile;
        break;

      default:
        RestoreInput();
        return True;
    }
  }

notCompiling:
  level = 0;

  while (1) {
    RestoreInput();
    if (!GetNewInputLine()) return False;
    SetInputToCurrentLine();

    switch (GetPreprocessorToken()) {
      case PT_IF:
      case PT_IFDEF:
      case PT_IFNDEF:
        ++level;
        break;

      case PT_ELIF:
        if (!level) {
          if (!GetNumber("Constant expression required")) setSymVal(0);
          if (symVal) goto compiling;
        }
        break;

      case PT_ELIFDEF:
        if (!level && GetDefineSymbol()) goto compiling;
        break;

      case PT_ELIFNDEF:
        if (!level && !GetDefineSymbol()) goto compiling;
        break;

      case PT_ELSE:
        if (!level) goto compiling;
        break;

      case PT_ENDIF:
        if (!level) {
          --nestedCondCompile;
          goto compiling;
        }
        --level;
        break;

      default:
        // If not recognized, continue to next token
        break;
    }
  }

gettingEndif:
  level = 0;

  while (1) {
    RestoreInput();
    if (!GetNewInputLine()) return False;
    SetInputToCurrentLine();

    switch (GetPreprocessorToken()) {
      case PT_IF:
      case PT_IFDEF:
      case PT_IFNDEF:
        level++;
        break;

      case PT_ENDIF:
        if (!level) {
          --nestedCondCompile;
          goto compiling;
        }
        --level;
        break;

      default:
        // If not recognized, continue to next token
        break;
    }
  }
}

static pt GetPreprocessorToken() {
  //	return which preprocessor token starts the line, using a very limited
  //	definition of 'token'

  struct {
    const char* text;
    pt token;
  } tokens[] = {{"#ifdef", PT_IFDEF},  //	put longer before shorter
                {"#ifndef", PT_IFNDEF},     {"#if", PT_IF},
                {"#elifdef", PT_ELIFDEF},  //		"               "
                {"#elifndef", PT_ELIFNDEF}, {"#elif", PT_ELIF},
                {"#else", PT_ELSE},         {"#endif", PT_ENDIF}};

  //	find first nonwhite
  const char* cp;
  for (cp = is->ptr; *cp && (*cp == ' ' || *cp == '\t'); cp++);

  //	has to start with #
  if (!*cp || *cp != '#') return PT_NONE;

  //	see if it matches any of the tokens
  for (size_t i = 0; i < sizeof tokens / sizeof *tokens; i++) {
    int len = strlen(tokens[i].text);
    if (!strncmp(cp, tokens[i].text, len)) {
      //	make sure that the full token matches
      cp += len;
      if (!*cp || *cp == '\n' || *cp == ' ' || *cp == '\t') {
        is->ptr = cp;
        return tokens[i].token;
      }
      break;
    }
  }

  return PT_NONE;
}

static void ReadNumber(strptr ip) {
  int c;
  int base;
  int sign;
  std::string_view validDigits;

  SCIWord val = 0;
  symStr.clear();

  // Determine the sign of the number
  if (*ip != '-')
    sign = 1;
  else {
    sign = -1;
    symStr.push_back(*ip++);
  }

  // Determine the base of the number
  base = 10;
  if (*ip == '%' || *ip == '$') {
    base = (*ip == '%') ? 2 : 16;
    symStr.push_back(*ip++);
  }
  switch (base) {
    case 2:
      validDigits = binDigits;
      break;
    case 10:
      validDigits = decDigits;
      break;
    case 16:
      validDigits = hexDigits;
      break;
  }

  while (!IsTerm(*ip)) {
    auto rawChar = *ip;
    c = absl::ascii_tolower(*ip);
    auto char_index = validDigits.find((char)c);
    if (char_index == std::string_view::npos) {
      Warning("Invalid character in number: %c (%d).  Number = %d. Index: %d",
              *ip, *ip, symVal, validDigits[4]);
      break;
    }
    val = SCIWord((val * base) + char_index);
    ++ip;
    symStr.push_back(rawChar);
  }

  val *= sign;

  setSymVal(val);

  is->ptr = ip;
  SetTokenEnd();
}

static void ReadString(strptr ip) {
  char c;
  char open;
  bool truncated;
  uint32_t n;

  truncated = False;
  symStr.clear();
  open = *ip++;

  symType = S_STRING;

  switch (open) {
    case ALT_QUOTE:
      symType = S_STRING;
      open = '}';  // Set closing character to be different
      break;
  }

  while ((c = *ip++) != open && c != '\0') {
    switch (c) {
      case '\n':
        GetNewLine();
        if (!is) Fatal("Unterminated string");
        ip = is->ptr;
        break;

      case '\r':
        break;

      case '_':
        if (!truncated) symStr.push_back(' ');
        break;

      case ' ':
      case '\t':
        if (!symStr.empty() && symStr.back() != '\n' && !truncated) {
          symStr.push_back(' ');
        }
        while ((c = *ip) == ' ' || c == '\t' || c == '\n') {
          ++ip;
          if (c == '\n') {
            GetNewLine();
            if (!is) Fatal("Unterminated string");
            ip = is->ptr;
          }
        }
        break;

      case '\\':
        if (!IsHex(c = *ip++)) {  // else move to next char
          // Then just use char as is.
          switch (c) {
            case 'n':
              if (!truncated) symStr.push_back('\n');
              break;
            case 't':
              if (!truncated) symStr.push_back('\t');
              break;
            case 'r':
              if (!truncated) {
                symStr.push_back('\r');
                symStr.push_back('\n');
              }
              break;
            default:
              if (!truncated) symStr.push_back(c);
              break;
          }

        } else {
          // Then insert a hex number in the string.

          c = absl::ascii_tolower(c);
          int high_digit = hexDigits.find(c);
          n = (uint32_t)high_digit * 16;
          c = *ip++;
          c = absl::ascii_tolower(c);
          int low_digit = hexDigits.find(c);
          n += (uint32_t)low_digit;
          if (!truncated) symStr.push_back((char)n);
        }

        break;

      default:
        if (!truncated) symStr.push_back(c);
        break;
    }

    if (symStr.length() >= MaxTokenLen && !truncated) {
      Error("String too large.");
      truncated = True;
    }
  }

  if (c == '\0') Error("Unterminated string");

  if (is) {
    is->ptr = ip;
    SetTokenEnd();
  } else {
    EarlyEnd();
  }
}

static uint32_t altKey[] = {
    30, 48, 46, 32, 18, 33, 34, 35, 23,  // a - i
    36, 37, 38, 50, 49, 24, 25, 16, 19,  // j - r
    31, 20, 22, 47, 17, 45, 21, 44       // s - z
};

static void ReadKey(strptr ip) {
  symType = S_NUM;

  symStr.clear();
  while (!IsTerm(*ip)) symStr.push_back(*ip++);

  char firstChar = symStr[0];

  switch (firstChar) {
    case '^': {
      // A control key.
      auto ctrlChar = symStr[1];
      if (isalpha(ctrlChar))
        setSymVal(toupper(ctrlChar) - 0x40);
      else
        Error("Not a valid control key: %s", symStr);
      break;
    }

    case '@': {
      // An alt-key.
      auto altChar = symStr[1];
      if (isalpha(altChar))
        setSymVal(altKey[toupper(altChar) - 'A'] << 8);
      else
        Error("Not a valid alt key: %s", symStr);
      break;
    }

    case '#': {
      // A function key.
      int num;
      if (!absl::SimpleAtoi(std::string_view(symStr).substr(1), &num)) {
        Error("Not a valid function key: %s", symStr);
        break;
      }
      setSymVal((num + 58) << 8);
      break;
    }

    default:
      // Just a character...
      setSymVal(firstChar);
      break;
  }

  is->ptr = ip;
  SetTokenEnd();
}
