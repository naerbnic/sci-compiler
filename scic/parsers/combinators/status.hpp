#ifndef PARSERS_COMBINATORS_STATUS_HPP
#define PARSERS_COMBINATORS_STATUS_HPP

#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/combinators/internal_util.hpp"

namespace parsers {

class ParseStatus {
 public:
  enum Kind {
    OK,
    FAILURE,
    FATAL,
  };

  static ParseStatus Ok() { return ParseStatus(OK, {}); }

  static ParseStatus Failure(std::vector<diag::Diagnostic> errors) {
    return ParseStatus(FAILURE, std::move(errors));
  }
  static ParseStatus Fatal(std::vector<diag::Diagnostic> errors) {
    return ParseStatus(FATAL, std::move(errors));
  }

  // Compose two errors together.
  ParseStatus operator|(ParseStatus const& other) const {
    if (kind_ == OK) {
      return other;
    }
    if (other.kind_ == OK) {
      return *this;
    }
    if (kind_ == other.kind_) {
      std::vector<diag::Diagnostic> messages = messages_;
      messages.insert(messages.end(), other.messages_.begin(),
                      other.messages_.end());
      return ParseStatus(kind_,
                         internal::ConcatVectors(messages_, other.messages_));
    }
    if (kind_ == FATAL) {
      return *this;
    }
    if (other.kind_ == FATAL) {
      return other;
    }
    throw std::runtime_error("Invalid error combination.");
  }

  ParseStatus() = default;

  Kind kind() const { return kind_; }
  absl::Span<diag::Diagnostic const> messages() const { return messages_; }
  bool ok() const { return kind_ == OK; }

 private:
  ParseStatus PrependDiagnostics(
      std::vector<diag::Diagnostic> messages) const& {
    return ParseStatus(kind_,
                       internal::ConcatVectors(std::move(messages), messages_));
  }

  ParseStatus&& PrependDiagnostics(std::vector<diag::Diagnostic> messages) && {
    messages_ = internal::ConcatVectors(std::move(messages), messages_);
    return std::move(*this);
  }

  ParseStatus AppendDiagnostics(std::vector<diag::Diagnostic> messages) const& {
    return ParseStatus(kind_,
                       internal::ConcatVectors(messages_, std::move(messages)));
  }

  ParseStatus&& AppendDiagnostics(std::vector<diag::Diagnostic> messages) && {
    messages_ = internal::ConcatVectors(messages_, std::move(messages));
    return std::move(*this);
  }

  ParseStatus(Kind kind, std::vector<diag::Diagnostic> messages)
      : kind_(kind), messages_(std::move(messages)) {}

  Kind kind_ = OK;
  std::vector<diag::Diagnostic> messages_;

  friend std::ostream& operator<<(std::ostream& os, ParseStatus const& status) {
    absl::Format(&os, "%v", status);
    return os;
  }

  template <class Sink>
  friend void AbslStringify(Sink& sink, ParseStatus const& status) {
    switch (status.kind()) {
      case ParseStatus::OK:
        sink.Append("OK");
        return;
      case ParseStatus::FAILURE:
        sink.Append("FAILURE");
        break;
      case ParseStatus::FATAL:
        sink.Append("FATAL");
        break;
    }

    sink.Append("\n");

    for (auto const& message : status.messages()) {
      sink.Append("  ");
      absl::Format(&sink, "%v", message);
      sink.Append("\n");
    }
  }
};
}  // namespace parsers

#endif