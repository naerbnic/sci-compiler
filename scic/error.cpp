//	error.cpp	sc
// 	error message routines for sc

#include "scic/error.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "absl/strings/str_format.h"
#include "scic/input.hpp"
#include "scic/symbol.hpp"
#include "scic/token.hpp"
#include "util/platform/platform.hpp"

int gNumErrors;
int gNumWarnings;

static void beep();

namespace {

void ListingOutput(std::string_view str) {
  error_impl::WriteOutput(str);
  // Listing("%s", str);
}

}  // namespace

[[noreturn]] void EarlyEnd() { Fatal("Unexpected end of input."); }

static void beep() { putc('\a', stderr); };

namespace error_impl {

void WriteError(std::string_view text) {
  ++gNumErrors;

  ListingOutput(absl::StrFormat("Error: %s, line %d\n\t",
                                gInputState.GetCurrFileName(),
                                gInputState.GetCurrLineNum()));
  ListingOutput(text);
  ListingOutput("\n");

  UnGetTok();
  auto token = GetToken();
  if (!CloseP(token.type()))
    (void)GetRest(true);
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

  UnGetTok();
  auto token = GetToken();
  if (!CloseP(token.type()))
    (void)GetRest(true);
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

}  // namespace error_impl
