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
#include <unordered_map>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "scic/diagnostics/formatter.hpp"
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

  virtual std::unique_ptr<internal::FormatArgs> format_args() const = 0;

 protected:
  using FieldReadMap = std::unordered_map<
      std::string_view,
      absl::AnyInvocable<absl::Status(Derived const&, std::string*) const>>;
  class FormatArgsImpl : public internal::FormatArgs {
   public:
    explicit FormatArgsImpl(Derived const* diag, FieldReadMap const*)
        : diag_(diag) {}

    absl::Status AppendArgument(std::string* target,
                                std::string_view arg_name) const override {
      return field_read_map_->at(arg_name)(*diag_, target);
    }

   private:
    Derived const* diag_;
    FieldReadMap const* field_read_map_;
  };

  template <class MemPtr, class... Args>
  static FieldReadMap MakeFieldReadMap(std::string_view field_name,
                                       MemPtr field_ptr, Args&&... args) {
    auto read_map = MakeFieldReadMap(std::forward<Args>(args)...);
    read_map[field_name] = [field_ptr](Derived const& diag,
                                       std::string* target) {
      auto const& value = (diag.*field_ptr);
      absl::Format(target, "%s", value);
      return absl::OkStatus();
    };
  }

  static FieldReadMap MakeFieldReadMap() { return {}; }

 private:
  static std::size_t Register() {
    // Register the diagnostic with the registry.
    auto* registry = DiagnosticRegistry::Get();
    if (registry) {
      registry->RegisterDiagnostic(Derived::ID, util::TypeName<Derived>());
    }
    return 0;
  }
  static inline std::size_t registerer_ = Register();
};

#define DEFINE_FORMAT_ARGS_FOR_EACH_0(macro, ty, arg1)
#define DEFINE_FORMAT_ARGS_FOR_EACH_1(macro, ty, arg1) macro(ty, arg1)
#define DEFINE_FORMAT_ARGS_FOR_EACH_2(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_1(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_3(macro, ty, arg1, ...) \
  macro(ty, rg1), DEFINE_FORMAT_ARGS_FOR_EACH_2(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_4(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_3(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_5(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_4(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_6(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_5(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_7(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_6(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_8(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_7(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_9(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_8(macro, ty, __VA_ARGS__)
#define DEFINE_FORMAT_ARGS_FOR_EACH_10(macro, ty, arg1, ...) \
  macro(ty, arg1), DEFINE_FORMAT_ARGS_FOR_EACH_9(macro, ty, __VA_ARGS__)

#define DEFINE_FORMAT_ARGS_FOR_EACH_GET_MACRO(_0, _1, _2, _3, _4, _5, _6, _7, \
                                              _8, _9, _10, N, ...)            \
  N

#define DEFINE_FORMAT_ARGS_FOR_EACH(macro, ty, ...)                 \
  DEFINE_FORMAT_ARGS_FOR_EACH_GET_MACRO(                            \
      __VA_ARGS__, DEFINE_FORMAT_ARGS_FOR_EACH_10,                  \
      DEFINE_FORMAT_ARGS_FOR_EACH_9, DEFINE_FORMAT_ARGS_FOR_EACH_8, \
      DEFINE_FORMAT_ARGS_FOR_EACH_7, DEFINE_FORMAT_ARGS_FOR_EACH_6, \
      DEFINE_FORMAT_ARGS_FOR_EACH_5, DEFINE_FORMAT_ARGS_FOR_EACH_4, \
      DEFINE_FORMAT_ARGS_FOR_EACH_3, DEFINE_FORMAT_ARGS_FOR_EACH_2, \
      DEFINE_FORMAT_ARGS_FOR_EACH_1, DEFINE_FORMAT_ARGS_FOR_EACH_0) \
  (macro, ty, __VA_ARGS__)

#define DEFINE_FORMAT_ARGS_ITEM_HELPER(ty, arg) #arg, ty ::arg

// Use the FOR_EACH pattern for the preprocessor
#define DEFINE_FORMAT_ARGS(ty, ...)                                          \
  std::unique_ptr<internal::FormatArgs> format_args() const override {       \
    static auto args =                                                       \
        DiagnosticBase::MakeFieldReadMap(DEFINE_FORMAT_ARGS_FOR_EACH(        \
            DEFINE_FORMAT_ARGS_ITEM_HELPER, ty __VA_OPT__(, ) __VA_ARGS__)); \
    return std::make_unique<FormatArgsImpl>(this, &args);                    \
  }

class DiagnosticImpl : public DiagnosticBase<DiagnosticImpl> {
 public:
  static constexpr auto ID = DiagnosticId::Create(0);

  DiagnosticImpl(DiagnosticKind kind, DiagMessage primary)
      : kind_(kind), primary_(std::move(primary)) {}

  DiagnosticKind kind() const override { return kind_; }
  DiagMessage primary() const override { return primary_; }

 private:
  DEFINE_FORMAT_ARGS(DiagnosticImpl);
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