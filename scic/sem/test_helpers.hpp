#ifndef SEM_TEST_HELPERS_HPP
#define SEM_TEST_HELPERS_HPP

#include <string>
#include <string_view>

#include "scic/sem/common.hpp"
#include "scic/text/text_range.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {
inline NameToken CreateTestNameToken(std::string_view name) {
  return NameToken(util::RefStr(name),
                   text::TextRange::OfString(std::string(name)));
}
}  // namespace sem
#endif