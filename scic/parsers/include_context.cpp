#include "scic/parsers/include_context.hpp"

#include <string_view>

#include "scic/status/status.hpp"
#include "scic/text/text_range.hpp"

namespace parsers {
namespace {

class EmptyIncludeContext : public IncludeContext {
 public:
  status::StatusOr<text::TextRange> LoadTextFromIncludePath(
      std::string_view path) const override {
    return status::UnimplementedError("No includes.");
  }
};

}  // namespace

IncludeContext const* IncludeContext::GetEmpty() {
  static EmptyIncludeContext empty_context;
  return &empty_context;
}

}  // namespace parsers