// Types for providing error messages as output from the compiler.
//
// This abstracts the error representation from the way it's eventually used.

#ifndef ERRORS_ERRORS_HPP
#define ERRORS_ERRORS_HPP

#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/text/text_range.hpp"

namespace diag {

class Diagnostic {
 public:
  enum Kind {
    ERROR,
    WARNING,
    INFO,
  };

  Diagnostic(Kind kind, std::optional<text::TextRange> text,
             std::string message)
      : kind_(kind), text_(std::move(text)), message_(std::move(message)) {}

  Kind kind() const { return kind_; }
  std::optional<text::TextRange> const& text() const { return text_; }
  std::string const& message() const { return message_; }

  template <class... Args>
  static Diagnostic RangeError(text::TextRange const& text,
                               absl::FormatSpec<Args...> spec, Args&&... args) {
    return Diagnostic(ERROR, text, absl::StrFormat(spec, args...));
  }

  template <class... Args>
  static Diagnostic RangeWarning(text::TextRange const& text,
                                 absl::FormatSpec<Args...> spec,
                                 Args&&... args) {
    return Diagnostic(WARNING, text, absl::StrFormat(spec, args...));
  }

  template <class... Args>
  static Diagnostic RangeInfo(text::TextRange const& text,
                              absl::FormatSpec<Args...> spec, Args&&... args) {
    return Diagnostic(INFO, text, absl::StrFormat(spec, args...));
  }

  template <class... Args>
  static Diagnostic Error(absl::FormatSpec<Args...> spec, Args&&... args) {
    return Diagnostic(ERROR, std::nullopt, absl::StrFormat(spec, args...));
  }

  template <class... Args>
  static Diagnostic Warning(absl::FormatSpec<Args...> spec, Args&&... args) {
    return Diagnostic(WARNING, std::nullopt, absl::StrFormat(spec, args...));
  }

  template <class... Args>
  static Diagnostic Info(absl::FormatSpec<Args...> spec, Args&&... args) {
    return Diagnostic(INFO, std::nullopt, absl::StrFormat(spec, args...));
  }

 private:
  Kind kind_;
  std::optional<text::TextRange> text_;
  std::string message_;
};

class DiagnosticsSink {
 public:
  virtual ~DiagnosticsSink() = default;

  virtual void AddDiagnostic(Diagnostic diagnostic) = 0;

  template <typename... Args>
  void Error(text::TextRange const& text, absl::FormatSpec<Args...> format,
             Args&&... args) {
    AddDiagnostic(Diagnostic::Error(text, format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void Warning(text::TextRange const& text, absl::FormatSpec<Args...> format,
               Args&&... args) {
    AddDiagnostic(
        Diagnostic::Warning(text, format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void Info(text::TextRange const& text, absl::FormatSpec<Args...> format,
            Args&&... args) {
    AddDiagnostic(Diagnostic::Info(text, format, std::forward<Args>(args)...));
  }
};

}  // namespace diag

#endif