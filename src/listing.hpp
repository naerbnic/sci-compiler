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
void ListOp(std::size_t offset, uint8_t);
template <class... Args>
void ListArg(absl::FormatSpec<Args...> fmt, Args const&... args);
template <class... Args>
void ListAsCode(std::size_t offset, absl::FormatSpec<Args...> fmt,
                Args const&... args);
void ListWord(std::size_t offset, uint16_t);
void ListByte(std::size_t offset, uint8_t);
void ListOffset(std::size_t offset);
void ListText(std::size_t offset, std::string_view);
template <class... Args>
void ListingNoCRLF(absl::FormatSpec<Args...> fmt, Args const&... args);
void ListSourceLine(int num);

extern bool listCode;

// Implementation Below

namespace listing_impl {

void ListingImpl(absl::FunctionRef<bool(FILE*)> print_func);
void ListAsCodeImpl(std::size_t offset,
                    absl::FunctionRef<bool(FILE*)> print_func);
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
void ListAsCode(std::size_t offset, absl::FormatSpec<Args...> fmt,
                Args const&... args) {
  listing_impl::ListAsCodeImpl(
      offset, [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}

template <class... Args>
void ListingNoCRLF(absl::FormatSpec<Args...> fmt, Args const&... args) {
  listing_impl::ListingNoCRLFImpl(
      [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}

#endif
