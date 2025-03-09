#include "scic/diagnostics/formatter.hpp"

#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/strip.h"
#include "util/status/status_macros.hpp"

namespace diag::internal {

absl::Status FormatString(std::string* target, std::string_view format,
                          FormatArgs const* args) {
  while (true) {
    if (absl::ConsumePrefix(&format, "$$")) {
      target->append("$");
      continue;
    }

    if (absl::ConsumePrefix(&format, "${")) {
      auto end = format.find('}');
      if (end == std::string_view::npos) {
        return absl::InvalidArgumentError("Missing closing brace");
      }
      std::string_view arg_name = format.substr(0, end);
      format.remove_prefix(end + 1);
      RETURN_IF_ERROR(args->AppendArgument(target, arg_name));
      continue;
    }

    auto pos = format.find('$');
    if (pos == std::string_view::npos) {
      target->append(format);
      break;
    } else {
      target->append(format.substr(0, pos));
      format.remove_prefix(pos);
    }
  }
  return absl::OkStatus();
}

}  // namespace diag::internal
