#ifndef DIAGNOSTICS_FORMATTER_HPP
#define DIAGNOSTICS_FORMATTER_HPP

#include <string>
#include <string_view>

#include "absl/status/status.h"

namespace diag::internal {

class FormatArgs {
 public:
  virtual ~FormatArgs() = default;
  virtual absl::Status AppendArgument(std::string* target,
                                      std::string_view arg_name) const = 0;
};

// Formats a string using simple variable substitution.
//
// We use "${arg_name}" to refer to an argument. If a dollar sign is
// needed, you can use $$.
absl::Status FormatString(std::string* target, std::string_view format,
                          FormatArgs const& args);

}  // namespace diag::internal

#endif