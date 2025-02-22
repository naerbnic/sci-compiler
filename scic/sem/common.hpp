#ifndef SEM_COMMON_HPP
#define SEM_COMMON_HPP

#include "absl/types/span.h"
#include "scic/parsers/sci/ast.hpp"

namespace sem {

namespace ast = parsers::sci;
using Items = absl::Span<ast::Item const>;
}  // namespace sem

#endif  // SEM_COMMON_HPP