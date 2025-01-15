//	error.cpp	sc
// 	error message routines for sc

#include "error.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include <exception>
#include <string>

// This may need to be define bound if used on a non-unix system.
#include <unistd.h>

#include "input.hpp"
#include "listing.hpp"
#include "sc.hpp"
#include "share.hpp"
#include "sol.hpp"
#include "string.hpp"
#include "symbol.hpp"
#include "token.hpp"

int errors;
int warnings;

static void beep();

namespace {

void InnerOutput(strptr str) {
  printf("%s", str);
  fflush(stdout);

  if (!isatty(fileno(stdout)) && isatty(fileno(stderr))) {
    fprintf(stderr, "%s", str);
  }
}

}  // namespace

void output(strptr fmt, ...) {
  va_list args;

  va_start(args, fmt);
  auto str = vstringf(fmt, args);
  va_end(args);

  InnerOutput(str.c_str());
}

void Info(strptr parms, ...) {
  va_list argPtr;

  std::string buf = stringf("Info: %s, line %d\n\t", curFile, curLine);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  va_start(argPtr, parms);
  buf = vstringf(parms, argPtr);
  va_end(argPtr);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  InnerOutput("\n");
  Listing("\n");
}

void Warning(strptr parms, ...) {
  va_list argPtr;

  ++warnings;

  std::string buf = stringf("Warning: %s, line %d\n\t", curFile, curLine);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  va_start(argPtr, parms);
  buf = vstringf(parms, argPtr);
  va_end(argPtr);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  InnerOutput("\n");
  Listing("\n");

  // Beep on first error/warning.

  if (warnings + errors == 1) beep();
}

void Error(strptr parms, ...) {
  va_list argPtr;

  ++errors;

  std::string buf = stringf("Error: %s, line %d\n\t", curFile, curLine);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  va_start(argPtr, parms);
  buf = vstringf(parms, argPtr);
  va_end(argPtr);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  InnerOutput("\n");
  Listing("\n");

  if (!CloseP(symType))
    GetRest(True);
  else
    UnGetTok();

  // Beep on first error/warning.

  if (warnings + errors == 1) beep();
}

void Severe(strptr parms, ...) {
  va_list argPtr;

  ++errors;

  auto buf = stringf("Error: %s, line %d\n\t", curFile, curLine);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  va_start(argPtr, parms);
  buf = vstringf(parms, argPtr);
  va_end(argPtr);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());

  InnerOutput("\n");
  Listing("\n");

  if (!CloseP(symType))
    GetRest(True);
  else
    UnGetTok();

  // Beep on first error/warning.

  if (warnings + errors == 1) beep();
}

[[noreturn]] void Fatal(strptr parms, ...) {
  va_list argPtr;

  std::string buf = stringf("Fatal: %s, line %d\n\t", curFile, curLine);
  InnerOutput(buf.c_str());
  Listing(buf.c_str());
  va_start(argPtr, parms);
  buf = vstringf(parms, argPtr);
  va_end(argPtr);
  InnerOutput(buf.c_str());
  // Previous implementation printed the format string, not the formatted
  // string.
  Listing(buf.c_str());
  InnerOutput("\n");
  Listing("\n");

  beep();

  CloseListFile();
  Unlock();
  exit(3);
}

[[noreturn]] void Panic(strptr parms, ...) {
  constexpr size_t bufSize = 200;
  va_list argPtr;
  char buf[bufSize];

  InnerOutput("Fatal: ");
  va_start(argPtr, parms);
  vsnprintf(buf, bufSize, parms, argPtr);
  InnerOutput(buf);
  va_end(argPtr);
  InnerOutput("\n");

  beep();

  CloseListFile();
  Unlock();
  exit(3);
}

void EarlyEnd() { Fatal("Unexpected end of input."); }

static void beep() { putc('\a', stderr); }

int AssertFail(char* file, int line, char* expression) {
  printf("Assertion failed in %s(%d): %s\n", file, line, expression);
  abort();

  // Return value is only useful for the way this function is used in
  // assert() macro under BC++.  However, WATCOM knows it's senseless.
  return 0;
}
