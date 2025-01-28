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
static void ReadKey(std::string_view ip);
static void ReadString(std::string_view ip);
static void ReadNumber(std::string_view ip);

static char currCharAndAdvance(std::string_view& ip) {
  if (ip.empty()) return '\0';
  auto c = ip.front();
  ip.remove_prefix(1);
  return c;
}

static char currChar(std::string_view ip) { return ip.empty() ? '\0' : ip[0]; }

static void advance(std::string_view& ip) {
  if (!ip.empty()) ip.remove_prefix(1);
}

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
    return true;
  }
  return false;
}

void UnGetTok() { haveUnGet = true; }

void GetRest(bool error) {
  // Copy the rest of the parenthesized expression into 'symStr'.
  //	If just seeking until next closing paren, don't display further
  //	messages.

  int pLevel;
  bool truncate;

  if (error && !is) return;

  std::string_view ip = is->inputPtr;
  symStr.clear();
  pLevel = 0;
  truncate = false;

  while (1) {
    if (ip.empty()) {
      CloseInputSource();
      if (!is) {
        if (!error) EarlyEnd();
        return;
      }
      ip = is->inputPtr;
      continue;
    }

    switch (ip[0]) {
      case '(':
        ++pLevel;
        break;

      case ')':
        if (pLevel > 0)
          --pLevel;
        else {
          symType = S_STRING;
          is->inputPtr = ip;
          SetTokenEnd();
          return;
        }
        break;

      case '\n':
        if (!is->incrementPastNewLine(ip)) EarlyEnd();
        continue;
    }

    if (!truncate) symStr.push_back(currCharAndAdvance(ip));

    if (symStr.length() >= MaxTokenLen && !truncate) {
      if (!error) Warning("Define too long.  Truncated.");
      truncate = true;
    }
  }
}

bool NextToken() {
  // Return the next token.  If we're at the end of the current input source,
  // close it and get input from the previous source in the queue.

  uint8_t c;  // the character

  if (haveUnGet) {
    haveUnGet = false;
    tokSym = lastTok;
    return true;
  }

  if (!is) {
    symType = S_END;
    return false;
  }

  // Get pointer to input in a register
  std::string_view ip = is->inputPtr;

  // Scan to the start of the next token.
  while (1) {
    if (ip.empty() || ip[0] == '\n') {
      if (is->endInputLine()) {
        ip = is->inputPtr;
        continue;
      } else {
        symType = S_END;
        return false;
      }
    }

    if (!IsSep(ip[0])) break;

    // Eat any whitespace.
    auto first_non_whitespace = ip.find_first_not_of(" \t");
    if (first_non_whitespace != std::string_view::npos) {
      ip.remove_prefix(first_non_whitespace);
    } else {
      ip = "";
      continue;
    }

    // If we hit the start of a comment, skip it.
    if (ip[0] == ';') {
      auto comment_end = ip.find('\n');
      if (comment_end != std::string_view::npos) {
        ip.remove_prefix(comment_end);
      } else {
        ip = "";
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

  c = ip[0];
  symStr.clear();

  if (IsTok(c)) {
    symStr.push_back(c);
    symType = (sym_t)c;
    is->inputPtr = ip.substr(1);
    SetTokenEnd();
    return true;
  }

  if (c == '`') {
    // A character constant.
    ReadKey(ip.substr(1));
    return true;
  }

  if (c == '"' || c == ALT_QUOTE) {
    ReadString(ip);
    return true;
  }

  if (IsDigit(c) || (c == '-' && ip.length() > 1 && IsDigit(ip[1]))) {
    symType = S_NUM;
    ReadNumber(ip);
    return true;
  }

  symType = S_IDENT;
  while (!IsTerm(c)) {
    ip.remove_prefix(1);
    if (c == ':') {
      // This is a selector literal (e.g. 'foo:'). Only include the part
      // before the quote, but mark the sym type.
      symType = S_SELECT_LIT;
      break;
    }
    if (IsIncl(c)) break;
    symStr.push_back(c);
    if (ip.empty()) {
      break;
    }
    c = ip[0];
  }
  is->inputPtr = ip;
  SetTokenEnd();

  return true;
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
    if (!GetNewInputLine()) return false;
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
        return true;
    }
  }

notCompiling:
  level = 0;

  while (1) {
    RestoreInput();
    if (!GetNewInputLine()) return false;
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
    if (!GetNewInputLine()) return false;
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
    std::string_view text;
    pt token;
  } tokens[] = {{"#ifdef", PT_IFDEF},  //	put longer before shorter
                {"#ifndef", PT_IFNDEF},     {"#if", PT_IF},
                {"#elifdef", PT_ELIFDEF},  //		"               "
                {"#elifndef", PT_ELIFNDEF}, {"#elif", PT_ELIF},
                {"#else", PT_ELSE},         {"#endif", PT_ENDIF}};

  //	find first nonwhite
  std::string_view cp = is->inputPtr;
  auto first_non_whitespace = cp.find_first_not_of(" \t");
  if (first_non_whitespace != std::string_view::npos) {
    cp.remove_prefix(first_non_whitespace);
  } else {
    cp = "";
  }

  //	has to start with #
  if (cp.empty() || !cp.starts_with('#')) return PT_NONE;

  //	see if it matches any of the tokens
  for (size_t i = 0; i < sizeof tokens / sizeof *tokens; i++) {
    if (cp.starts_with(tokens[i].text)) {
      //	make sure that the full token matches
      cp.remove_prefix(tokens[i].text.length());
      if (cp.empty() || cp[0] == '\n' || cp[0] == ' ' || cp[0] == '\t') {
        is->inputPtr = cp;
        return tokens[i].token;
      }
      break;
    }
  }

  return PT_NONE;
}

static void ReadNumber(std::string_view ip) {
  int c;
  int base;
  int sign;
  std::string_view validDigits;

  SCIWord val = 0;
  symStr.clear();

  // Determine the sign of the number
  if (currChar(ip) != '-')
    sign = 1;
  else {
    sign = -1;
    symStr.push_back(currCharAndAdvance(ip));
  }

  // Determine the base of the number
  base = 10;
  if (currChar(ip) == '%' || currChar(ip) == '$') {
    base = (currChar(ip) == '%') ? 2 : 16;
    symStr.push_back(currCharAndAdvance(ip));
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

  while (!IsTerm(currChar(ip))) {
    auto rawChar = currChar(ip);
    c = absl::ascii_tolower(rawChar);
    auto char_index = validDigits.find((char)c);
    if (char_index == std::string_view::npos) {
      Warning("Invalid character in number: %c.  Number = %d", rawChar, symVal);
      break;
    }
    val = SCIWord((val * base) + char_index);
    advance(ip);
    symStr.push_back(rawChar);
  }

  val *= sign;

  setSymVal(val);

  is->inputPtr = ip;
  SetTokenEnd();
}

static void ReadString(std::string_view ip) {
  char c = '\0';
  char open;
  bool truncated;
  uint32_t n;

  truncated = false;
  symStr.clear();
  open = currCharAndAdvance(ip);

  symType = S_STRING;

  switch (open) {
    case ALT_QUOTE:
      symType = S_STRING;
      open = '}';  // Set closing character to be different
      break;
  }

  while ((c = currCharAndAdvance(ip)) != open && c != '\0') {
    switch (c) {
      case '\n':
        GetNewLine();
        if (!is) Fatal("Unterminated string");
        ip = is->inputPtr;
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
        while ((c = currChar(ip)) == ' ' || c == '\t' || c == '\n') {
          advance(ip);
          if (c == '\n') {
            GetNewLine();
            if (!is) Fatal("Unterminated string");
            ip = is->inputPtr;
          }
        }
        break;

      case '\\':
        if (!IsHex(c = currCharAndAdvance(ip))) {  // else move to next char
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
          c = currCharAndAdvance(ip);
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
      truncated = true;
    }
  }

  if (c == '\0') Error("Unterminated string");

  if (is) {
    is->inputPtr = ip;
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

static void ReadKey(std::string_view ip) {
  symType = S_NUM;

  symStr.clear();
  while (!IsTerm(currChar(ip))) symStr.push_back(currCharAndAdvance(ip));

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

  is->inputPtr = ip;
  SetTokenEnd();
}
