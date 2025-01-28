// listing.hpp

#ifndef LISTING_HPP
#define LISTING_HPP

#include <cstdint>
#include <string_view>

#include "absl/functional/function_ref.h"
#include "absl/strings/str_format.h"

void OpenListFile(std::string_view sourceFileName);
void CloseListFile();
void DeleteListFile();
template <class... Args>
void Listing(absl::FormatSpec<Args...> fmt, Args const&... args);
void ListOp(uint8_t);
template <class... Args>
void ListArg(absl::FormatSpec<Args...> fmt, Args const&... args);
template <class... Args>
void ListAsCode(absl::FormatSpec<Args...> fmt, Args const&... args);
void ListWord(uint16_t);
void ListByte(uint8_t);
void ListOffset();
void ListText(std::string_view);
template <class... Args>
void ListingNoCRLF(absl::FormatSpec<Args...> fmt, Args const&... args);
void ListSourceLine(int num);

extern bool listCode;

// Implementation Below

namespace listing_impl {

void ListingImpl(absl::FunctionRef<bool(FILE*)> print_func);
void ListAsCodeImpl(absl::FunctionRef<bool(FILE*)> print_func);
void ListingNoCRLFImpl(absl::FunctionRef<bool(FILE*)> print_func);

}  // namespace listing_impl

template <class... Args>
void Listing(absl::FormatSpec<Args...> fmt, Args const&... args) {
  listing_impl::ListingImpl(
      [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}

template <class... Args>
void ListArg(absl::FormatSpec<Args...> fmt, Args const&... args) {
  Listing("\t%s", absl::StrFormat(fmt, args...));
}

template <class... Args>
void ListAsCode(absl::FormatSpec<Args...> fmt, Args const&... args) {
  listing_impl::ListAsCodeImpl(
      [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}

template <class... Args>
void ListingNoCRLF(absl::FormatSpec<Args...> fmt, Args const&... args) {
  listing_impl::ListingNoCRLFImpl(
      [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}

#endif
