// listing.hpp

#ifndef LISTING_HPP
#define LISTING_HPP

#include <absl/strings/str_format.h>

#include <cstdint>

#include "error.hpp"

void OpenListFile(const char* sourceFileName);
void CloseListFile();
void DeleteListFile();

template <class... Args>
void Listing(absl::FormatSpec<Args...> const& spec, Args const&... args);

void ListOp(bool);

template <class... Args>
void ListArg(absl::FormatSpec<Args...> const& spec, Args const&... args);

template <class... Args>
void ListAsCode(absl::FormatSpec<Args...> const& spec, Args const&... args);

void ListWord(uint16_t);
void ListByte(uint8_t);
void ListOffset();
void ListText(const char*);
void ListingNoCRLF(const char* parms, ...);
void ListSourceLine(int num);

extern FILE* listFile;
extern bool listCode;

// Implementations

template <class... Args>
void Listing(absl::FormatSpec<Args...> const& spec, Args const&... args) {
  if (!listCode || !listFile) return;

  if (absl::FPrintF(listFile, spec, args...) == EOF ||
      putc('\n', listFile) == EOF) {
    Panic("Error writing list file");
  }
}

template <class... Args>
void ListArg(absl::FormatSpec<Args...> const& spec, Args const&... args) {
  if (!listCode || !listFile) return;

  auto argStr = absl::StrFormat(spec, args...);
  Listing("\t%s", argStr);
}

template <class... Args>
void ListAsCode(absl::FormatSpec<Args...> const& spec, Args const&... args) {
  if (!listCode || !listFile) return;

  ListOffset();
  Listing(spec, args...);
}

#endif
