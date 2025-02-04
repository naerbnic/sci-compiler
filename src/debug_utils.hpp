#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <source_location>

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"

template <class... Args>
void DebugPrintImpl(std::source_location source_loc,
                    absl::FormatSpec<Args...> const& spec,
                    Args const&... args) {
  absl::FPrintF(stderr, "%s:%d: ", source_loc.file_name(), source_loc.line());
  absl::FPrintF(stderr, spec, args...);
}

#define DebugPrint(...) \
  DebugPrintImpl(std::source_location::current(), __VA_ARGS__)

class Escaped {
 public:
  explicit Escaped(std::string_view str) : str_(str) {}

 private:
  std::string_view str_;
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Escaped& value) {
    sink.Append("\"");
    sink.Append(absl::CEscape(value.str_));
    sink.Append("\"");
  }
};

#endif