#ifndef PARSERS_INCLUDE_CONTEXT_HPP
#define PARSERS_INCLUDE_CONTEXT_HPP

#include <string_view>

#include "absl/status/statusor.h"
#include "scic/text/text_range.hpp"

namespace parsers {

class IncludeContext {
 public:
  static IncludeContext const* GetEmpty();

  virtual ~IncludeContext() = default;

  virtual absl::StatusOr<text::TextRange> LoadTextFromIncludePath(
      std::string_view path) const = 0;
};

}  // namespace parsers
#endif