#ifndef CODEGEN_LISTING_HPP
#define CODEGEN_LISTING_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string_view>

#include "absl/functional/function_ref.h"
#include "absl/strings/str_format.h"

namespace codegen {

// Internal class
class ListingFileSink;

class ListingFile {
 public:
  static std::unique_ptr<ListingFile> Open(
      std::filesystem::path sourceFileName);
  static std::unique_ptr<ListingFile> Null();

  virtual ~ListingFile() = default;

  template <class... Args>
  void Listing(absl::FormatSpec<Args...> fmt, Args const&... args);
  template <class... Args>
  void ListArg(absl::FormatSpec<Args...> fmt, Args const&... args);
  template <class... Args>
  void ListAsCode(std::size_t offset, absl::FormatSpec<Args...> fmt,
                  Args const&... args);
  template <class... Args>
  void ListingNoCRLF(absl::FormatSpec<Args...> fmt, Args const&... args);

  void ListOp(std::size_t offset, std::uint8_t);
  void ListWord(std::size_t offset, std::uint16_t);
  void ListByte(std::size_t offset, std::uint8_t);
  void ListOffset(std::size_t offset);
  void ListText(std::size_t offset, std::string_view);

 protected:
  friend class ListingFileSink;

  virtual bool Write(std::string_view) = 0;

  void ListingImpl(absl::FunctionRef<bool(absl::FormatRawSink)> print_func);
  void ListAsCodeImpl(std::size_t offset,
                      absl::FunctionRef<bool(absl::FormatRawSink)> print_func);
  void ListingNoCRLFImpl(
      absl::FunctionRef<bool(absl::FormatRawSink)> print_func);
};

// Implementation Below

template <class... Args>
void ListingFile::Listing(absl::FormatSpec<Args...> fmt, Args const&... args) {
  ListingImpl([&](absl::FormatRawSink fp) {
    return absl::Format(fp, fmt, args...) != EOF;
  });
}
template <class... Args>
void ListingFile::ListArg(absl::FormatSpec<Args...> fmt, Args const&... args) {
  Listing("\t%s", absl::StrFormat(fmt, args...));
}
template <class... Args>
void ListingFile::ListAsCode(std::size_t offset, absl::FormatSpec<Args...> fmt,
                             Args const&... args) {
  ListAsCodeImpl(offset, [&](absl::FormatRawSink fp) {
    return absl::Format(fp, fmt, args...) != EOF;
  });
}
template <class... Args>
void ListingFile::ListingNoCRLF(absl::FormatSpec<Args...> fmt,
                                Args const&... args) {
  ListingNoCRLFImpl([&](absl::FormatRawSink fp) {
    return absl::Format(fp, fmt, args...) != EOF;
  });
}
}  // namespace codegen
#endif