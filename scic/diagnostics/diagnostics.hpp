// Types for providing error messages as output from the compiler.
//
// This abstracts the error representation from the way it's eventually used.

#ifndef ERRORS_ERRORS_HPP
#define ERRORS_ERRORS_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token_source.hpp"
#include "util/types/name.hpp"
#include "util/types/strong_types.hpp"

namespace diag {

class DiagMessage {
 public:
  DiagMessage() = default;

  DiagMessage(std::string message, std::optional<tokens::TokenSource> source)
      : message_(std::move(message)), source_(std::move(source)) {}

  std::string const& message() const { return message_; }
  std::optional<text::FileRange> use_range() const {
    if (source_) {
      return source_->use_range().GetRange();
    }
    return std::nullopt;
  }

 private:
  std::string message_;
  std::optional<tokens::TokenSource> source_;
};

enum DiagnosticKind {
  ERROR,
  WARNING,
  INFO,
};

struct DiagnosticIdTag {
  using Value = std::uint64_t;
  static constexpr bool is_const = true;
};
using DiagnosticId = util::StrongValue<DiagnosticIdTag>;

class DiagnosticRegistry {
 public:
  static DiagnosticRegistry* Get();

  virtual ~DiagnosticRegistry() = default;

  virtual void RegisterDiagnostic(DiagnosticId id,
                                  std::string_view type_name) = 0;
};

class DiagnosticInterface {
 public:
  virtual ~DiagnosticInterface() = default;

  virtual DiagnosticId id() const = 0;
  virtual DiagnosticKind kind() const = 0;
  virtual DiagMessage primary() const = 0;
};

// A CRTP class to define new diagnostics.
//
// Subclasses have to define a static constexpr ID field with a
// DiagnosticId value. This ID will be registered with the
// DiagnosticRegistry when the subclass is instantiated, checking
// that each ID is unique.
template <class Derived>
class DiagnosticBase : public DiagnosticInterface {
 public:
  DiagnosticId id() const final {
    // A hack: Ensure the registerer_ is linked in by using its
    // value here.
    if (registerer_ != 0) {
      throw std::runtime_error("DiagnosticBase::registerer_ is not linked");
    }
    return Derived::ID;
  }

 private:
  static inline std::size_t registerer_ = ([] {
    // Register the diagnostic with the registry.
    auto* registry = DiagnosticRegistry::Get();
    if (registry) {
      registry->RegisterDiagnostic(Derived::ID, util::TypeName<Derived>());
    }
    return 0;
  })();
};

class DiagnosticImpl : public DiagnosticBase<DiagnosticImpl> {
 public:
  static constexpr auto ID = DiagnosticId::Create(0);

  DiagnosticImpl(DiagnosticKind kind, DiagMessage primary)
      : kind_(kind), primary_(std::move(primary)) {}

  DiagnosticKind kind() const override { return kind_; }
  DiagMessage primary() const override { return primary_; }

 private:
  DiagnosticKind kind_;
  DiagMessage primary_;
};

class Diagnostic {
 public:
  using Kind = DiagnosticKind;

  Diagnostic(Kind kind, DiagMessage primary)
      : impl_(std::make_shared<DiagnosticImpl>(kind, std::move(primary))) {}

  template <std::derived_from<DiagnosticInterface> T>
  Diagnostic(T&& impl) : impl_(std::make_shared<T>(std::forward(impl))) {}

  Kind kind() const { return impl_->kind(); }
  DiagMessage primary() const { return impl_->primary(); }

  template <class... Args>
  static Diagnostic RangeError(text::TextRange const& text,
                               absl::FormatSpec<Args...> const& spec,
                               Args const&... args) {
    return Diagnostic(ERROR, DiagMessage(absl::StrFormat(spec, args...), text));
  }

  template <class... Args>
  static Diagnostic RangeWarning(text::TextRange const& text,
                                 absl::FormatSpec<Args...> const& spec,
                                 Args const&... args) {
    return Diagnostic(WARNING,
                      DiagMessage(absl::StrFormat(spec, args...), text));
  }

  template <class... Args>
  static Diagnostic RangeInfo(text::TextRange const& text,
                              absl::FormatSpec<Args...> const& spec,
                              Args const&... args) {
    return Diagnostic(INFO, DiagMessage(absl::StrFormat(spec, args...), text));
  }

  template <class... Args>
  static Diagnostic Error(absl::FormatSpec<Args...> const& spec,
                          Args const&... args) {
    return Diagnostic(
        ERROR, DiagMessage(absl::StrFormat(spec, args...), std::nullopt));
  }

  template <class... Args>
  static Diagnostic Warning(absl::FormatSpec<Args...> const& spec,
                            Args const&... args) {
    return Diagnostic(
        WARNING, DiagMessage(absl::StrFormat(spec, args...), std::nullopt));
  }

  template <class... Args>
  static Diagnostic Info(absl::FormatSpec<Args...> const& spec,
                         Args const&... args) {
    return Diagnostic(
        INFO, DiagMessage(absl::StrFormat(spec, args...), std::nullopt));
  }

 private:
  std::shared_ptr<DiagnosticInterface> impl_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, Diagnostic const& diag) {
    switch (diag.kind()) {
      case ERROR:
        sink.Append("error: ");
        break;
      case WARNING:
        sink.Append("warning: ");
        break;
      case INFO:
        sink.Append("info: ");
        break;
    }

    auto primary = diag.primary();
    auto range = primary.use_range();

    if (range) {
      absl::Format(&sink, "%s:%d:%d", range->filename(),
                   range->start().line_index() + 1,
                   range->start().column_index() + 1);
    }

    absl::Format(&sink, ": %s", primary.message());
  }

  friend std::ostream& operator<<(std::ostream& os, Diagnostic const& diag) {
    auto primary = diag.primary();
    auto range = primary.use_range();
    if (range) {
      os << range->filename() << ":" << (range->start().line_index() + 1) << ":"
         << (range->start().column_index() + 1);
      if (range->end().line_index() != range->start().line_index()) {
        os << "-" << (range->end().line_index() + 1) << ":"
           << (range->end().column_index() + 1);
      } else if (range->end().column_index() != range->start().column_index()) {
        os << "-" << (range->end().column_index() + 1);
      }
      os << ": ";
    }
    switch (diag.kind()) {
      case ERROR:
        os << "error: ";
        break;
      case WARNING:
        os << "warning: ";
        break;
      case INFO:
        os << "info: ";
        break;
    }

    return os << primary.message();
  }
};

class DiagnosticsSink {
 public:
  virtual ~DiagnosticsSink() = default;

  virtual void AddDiagnostic(Diagnostic diagnostic) = 0;

  template <typename... Args>
  void Error(absl::FormatSpec<Args...> format, Args&&... args) {
    AddDiagnostic(Diagnostic::Error(format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void Warning(absl::FormatSpec<Args...> format, Args&&... args) {
    AddDiagnostic(Diagnostic::Warning(format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void Info(absl::FormatSpec<Args...> format, Args&&... args) {
    AddDiagnostic(Diagnostic::Info(format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void RangeError(text::TextRange const& text, absl::FormatSpec<Args...> format,
                  Args&&... args) {
    AddDiagnostic(
        Diagnostic::RangeError(text, format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void RangeWarning(text::TextRange const& text,
                    absl::FormatSpec<Args...> format, Args&&... args) {
    AddDiagnostic(
        Diagnostic::RangeWarning(text, format, std::forward<Args>(args)...));
  }

  template <typename... Args>
  void RangeInfo(text::TextRange const& text, absl::FormatSpec<Args...> format,
                 Args&&... args) {
    AddDiagnostic(
        Diagnostic::RangeInfo(text, format, std::forward<Args>(args)...));
  }
};

}  // namespace diag

#endif