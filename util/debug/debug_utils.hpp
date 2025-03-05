#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <array>
#include <cstdio>
#include <source_location>
#include <string_view>

#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"

template <class... Args>
void DebugPrintImpl(std::source_location source_loc,
                    absl::FormatSpec<Args...> const& spec,
                    Args const&... args) {
  absl::FPrintF(stderr, "%s:%d: ", source_loc.file_name(), source_loc.line());
  absl::FPrintF(stderr, spec, args...);
  absl::FPrintF(stderr, "\n");
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

inline void PrintStackTrace() {
  std::array<void*, 64> stack;
  int depth = absl::GetStackTrace(stack.data(), stack.size(), 0);
  std::array<char, 512> symbol_buffer;
  for (int i = 0; i < depth; ++i) {
    if (absl::Symbolize(stack[i], symbol_buffer.data(), symbol_buffer.size())) {
      absl::FPrintF(stderr, "%s\n", symbol_buffer.data());
    } else {
      absl::FPrintF(stderr, "??\n");
    }
  }
}

#endif