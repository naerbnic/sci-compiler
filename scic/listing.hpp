// listing.hpp

#ifndef LISTING_HPP
#define LISTING_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>

#include "absl/functional/function_ref.h"
#include "absl/strings/str_format.h"

void OpenListFile(std::string_view sourceFileName);
void CloseListFile();
void DeleteListFile();

class ListingFile {
 public:
  static std::unique_ptr<ListingFile> Open(int scriptNum,
                                           std::string_view sourceFileName);
  static std::unique_ptr<ListingFile> Null();

  ListingFile() = default;
  virtual ~ListingFile() = default;

  ListingFile(const ListingFile&) = delete;

  template <class... Args>
  void Listing(absl::FormatSpec<Args...> fmt, Args const&... args);
  template <class... Args>
  void ListArg(absl::FormatSpec<Args...> fmt, Args const&... args);
  template <class... Args>
  void ListAsCode(std::size_t offset, absl::FormatSpec<Args...> fmt,
                  Args const&... args);
  template <class... Args>
  void ListingNoCRLF(absl::FormatSpec<Args...> fmt, Args const&... args);

  virtual void ListOp(std::size_t offset, uint8_t) = 0;
  virtual void ListWord(std::size_t offset, uint16_t) = 0;
  virtual void ListByte(std::size_t offset, uint8_t) = 0;
  virtual void ListOffset(std::size_t offset) = 0;
  virtual void ListText(std::size_t offset, std::string_view) = 0;
  virtual void ListSourceLine(int num) = 0;

 protected:
  virtual void ListingImpl(absl::FunctionRef<bool(FILE*)> print_func) = 0;
  virtual void ListAsCodeImpl(std::size_t offset,
                              absl::FunctionRef<bool(FILE*)> print_func) = 0;
  virtual void ListingNoCRLFImpl(absl::FunctionRef<bool(FILE*)> print_func) = 0;
};

// Implementation Below

template <class... Args>
void ListingFile::Listing(absl::FormatSpec<Args...> fmt, Args const&... args) {
  ListingImpl([&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}
template <class... Args>
void ListingFile::ListArg(absl::FormatSpec<Args...> fmt, Args const&... args) {
  Listing("\t%s", absl::StrFormat(fmt, args...));
}
template <class... Args>
void ListingFile::ListAsCode(std::size_t offset, absl::FormatSpec<Args...> fmt,
                             Args const&... args) {
  ListAsCodeImpl(
      offset, [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}
template <class... Args>
void ListingFile::ListingNoCRLF(absl::FormatSpec<Args...> fmt,
                                Args const&... args) {
  ListingNoCRLFImpl(
      [&](FILE* fp) { return absl::FPrintF(fp, fmt, args...) != EOF; });
}

#endif
