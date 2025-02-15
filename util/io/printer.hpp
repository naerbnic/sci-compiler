#ifndef UTIL_IO_PRINTER_HPP
#define UTIL_IO_PRINTER_HPP

#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string_view>
#include <vector>

#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/str_format.h"
#include "util/types/name.hpp"

namespace util {
namespace internal {

template <class Sink, class T>
  requires(absl::HasAbslStringify<T>::value)
void PrintAny(Sink& sink, T const& value) {
  absl::Format(&sink, "%v", value);
}

template <class Sink, class T>
  requires(!absl::HasAbslStringify<T>::value &&
           requires(T const& t, std::ostream& os) {
             { os << t } -> std::same_as<std::ostream&>;
           })
void PrintAny(Sink& sink, T const& value) {
  std::stringstream ss;
  ss << value;
  absl::Format(&sink, "%s", ss.str());
}

template <class Sink, class T>
  requires(!absl::HasAbslStringify<T>::value &&
           !requires(T const& t, std::ostream& os) {
             { os << t } -> std::same_as<std::ostream&>;
           })
void PrintAny(Sink& sink, T const& value) {
  absl::Format(&sink, "<%s value>", TypeName<T>());
}

template <class Sink, class T>
void PrintAny(Sink& sink, std::unique_ptr<T> const& value) {
  if (!value.get()) {
    absl::Format(&sink, "nullptr");
  } else {
    PrintAny(sink, *value);
  }
}

template <class Sink, class T>
void PrintAny(Sink& sink, std::vector<T> const& value) {
  sink.Append("[");
  bool first = true;
  for (auto const& elem : value) {
    if (!first) {
      sink.Append(", ");
    }
    PrintAny(sink, elem);
    first = false;
  }
  sink.Append("]");
}

template <class Sink, class T>
void PrintAny(Sink& sink, std::optional<T> const& value) {
  if (value.has_value()) {
    PrintAny(sink, value.value());
  } else {
    absl::Format(&sink, "nullopt");
  }
}

template <class Sink>
void InnerPrinter(Sink& sink) {
  // Do nothing;
}

template <class Sink, class T, class... Args>
void InnerPrinter(Sink& sink, std::string_view field_name, T const& value,
                  Args&&... args) {
  absl::Format(&sink, "%s: ", field_name);
  PrintAny(sink, value);
  if (sizeof...(args) > 0) {
    absl::Format(&sink, ", ");
  }
  InnerPrinter(sink, std::forward<Args>(args)...);
}

}  // namespace internal

template <class Sink, class... Args>
void PrintStruct(Sink& sink, std::string_view struct_name, Args&&... args) {
  absl::Format(&sink, "%s(", struct_name);
  internal::InnerPrinter(sink, std::forward<Args>(args)...);
  absl::Format(&sink, ")");
}

#define DEFINE_PRINTERS(struct_name, ...)                               \
  template <class Sink>                                                 \
  void AbslStringifyImpl(Sink& sink) const {                            \
    ::util::PrintStruct(sink, #struct_name __VA_OPT__(, ) __VA_ARGS__); \
  }                                                                     \
                                                                        \
  template <class Sink>                                                 \
  friend void AbslStringify(Sink& sink, struct_name const& value) {     \
    value.AbslStringifyImpl(sink);                                      \
  }                                                                     \
                                                                        \
  friend std::ostream& operator<<(std::ostream& os,                     \
                                  struct_name const& value) {           \
    os << absl::StrFormat("%v", value);                                 \
    return os;                                                          \
  }

}  // namespace util
#endif