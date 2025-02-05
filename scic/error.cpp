//	error.cpp	sc
// 	error message routines for sc

#include "scic/error.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include <string_view>

#include "absl/strings/str_format.h"
#include "scic/input.hpp"
#include "scic/platform.hpp"
#include "scic/share.hpp"
#include "scic/symbol.hpp"
#include "scic/token.hpp"

int gNumErrors;
int gNumWarnings;

static void beep();

namespace {

void ListingOutput(std::string_view str) {
  error_impl::WriteOutput(str);
  // Listing("%s", str);
}

}  // namespace

void EarlyEnd() { Fatal("Unexpected end of input."); }

static void beep() { putc('\a', stderr); };

namespace error_impl {

void WriteError(std::string_view text) {
  ++gNumErrors;

  ListingOutput(absl::StrFormat("Error: %s, line %d\n\t",
                                gInputState.GetCurrFileName(),
                                gInputState.GetCurrLineNum()));
  ListingOutput(text);
  ListingOutput("\n");

  if (!CloseP(symType()))
    GetRest(true);
  else
    UnGetTok();

  // Beep on first error/warning.

  if (gNumWarnings + gNumErrors == 1) beep();
}

[[noreturn]] void WriteFatal(std::string_view text) {
  ListingOutput(absl::StrFormat("Fatal: %s, line %d\n\t",
                                gInputState.GetCurrFileName(),
                                gInputState.GetCurrLineNum()));
  ListingOutput(text);
  ListingOutput("\n");

  beep();

  Unlock();
  exit(3);
}

void WriteInfo(std::string_view text) {
  ListingOutput(absl::StrFormat("Info: %s, line %d\n\t",
                                gInputState.GetCurrFileName(),
                                gInputState.GetCurrLineNum()));
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
  ++gNumErrors;

  ListingOutput(absl::StrFormat("Error: %s, line %d\n\t",
                                gInputState.GetCurrFileName(),
                                gInputState.GetCurrLineNum()));
  ListingOutput(text);
  ListingOutput("\n");

  if (!CloseP(symType()))
    GetRest(true);
  else
    UnGetTok();

  // Beep on first error/warning.

  if (gNumWarnings + gNumErrors == 1) beep();
}

void WriteWarning(std::string_view text) {
  ++gNumWarnings;

  ListingOutput(absl::StrFormat("Warning: %s, line %d\n\t",
                                gInputState.GetCurrFileName(),
                                gInputState.GetCurrLineNum()));
  ListingOutput(text);
  ListingOutput("\n");

  // Beep on first error/warning.

  if (gNumWarnings + gNumErrors == 1) beep();
}

[[noreturn]] void WritePanic(std::string_view text) {
  error_impl::WriteOutput("Fatal: ");
  error_impl::WriteOutput(text);
  error_impl::WriteOutput("\n");

  beep();

  Unlock();
  exit(3);
}

}  // namespace error_impl
