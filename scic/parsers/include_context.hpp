#ifndef PARSERS_INCLUDE_CONTEXT_HPP
#define PARSERS_INCLUDE_CONTEXT_HPP

#include <string_view>

#include "scic/status/status.hpp"
#include "scic/text/text_range.hpp"

namespace parsers {

class IncludeContext {
 public:
  static IncludeContext const* GetEmpty();

  virtual ~IncludeContext() = default;

  virtual status::StatusOr<text::TextRange> LoadTextFromIncludePath(
      std::string_view path) const = 0;
};

}  // namespace parsers
#endif