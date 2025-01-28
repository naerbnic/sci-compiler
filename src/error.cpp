//	error.cpp	sc
// 	error message routines for sc

#include "error.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include <string_view>

#include "absl/strings/str_format.h"
#include "input.hpp"
#include "listing.hpp"
#include "platform.hpp"
#include "share.hpp"
#include "symbol.hpp"
#include "token.hpp"

int errors;
int warnings;

static void beep();

namespace {

void ListingOutput(std::string_view str) {
  error_impl::WriteOutput(str);
  Listing("%s", str);
}

}  // namespace

void EarlyEnd() { Fatal("Unexpected end of input."); }

static void beep() { putc('\a', stderr); };

namespace error_impl {

void WriteError(std::string_view text) {
  ++errors;

  ListingOutput(absl::StrFormat("Error: %s, line %d\n\t", curFile, curLine));
  ListingOutput(text);
  ListingOutput("\n");

  if (!CloseP(symType))
    GetRest(true);
  else
    UnGetTok();

  // Beep on first error/warning.

  if (warnings + errors == 1) beep();
}

[[noreturn]] void WriteFatal(std::string_view text) {
  ListingOutput(absl::StrFormat("Fatal: %s, line %d\n\t", curFile, curLine));
  ListingOutput(text);
  ListingOutput("\n");

  beep();

  CloseListFile();
  Unlock();
  exit(3);
}

void WriteInfo(std::string_view text) {
  ListingOutput(absl::StrFormat("Info: %s, line %d\n\t", curFile, curLine));
  ListingOutput(text);
  ListingOutput("\n");
}

void WriteOutput(std::string_view str) {
  absl::PrintF("%s", str);
  fflush(stdout);

  if (!IsTTY(stdout) && IsTTY(stderr)) {
    absl::FPrintF(stderr, "%s", str);
  }
}

void WriteSevere(std::string_view text) {
  ++errors;

  ListingOutput(absl::StrFormat("Error: %s, line %d\n\t", curFile, curLine));
  ListingOutput(text);
  ListingOutput("\n");

  if (!CloseP(symType))
    GetRest(true);
  else
    UnGetTok();

  // Beep on first error/warning.

  if (warnings + errors == 1) beep();
}

void WriteWarning(std::string_view text) {
  ++warnings;

  ListingOutput(absl::StrFormat("Warning: %s, line %d\n\t", curFile, curLine));
  ListingOutput(text);
  ListingOutput("\n");

  // Beep on first error/warning.

  if (warnings + errors == 1) beep();
}

[[noreturn]] void WritePanic(std::string_view text) {
  error_impl::WriteOutput("Fatal: ");
  error_impl::WriteOutput(text);
  error_impl::WriteOutput("\n");

  beep();

  CloseListFile();
  Unlock();
  exit(3);
}

}  // namespace error_impl
