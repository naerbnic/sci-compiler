#ifndef SEM_PASSES_INITIAL_PASS_HPP
#define SEM_PASSES_INITIAL_PASS_HPP

#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/sci/ast.hpp"

namespace sem::passes {

class ModuleEnvironment {};

ModuleEnvironment RunInitialPass(absl::Span<parsers::sci::Item const> items,
                                 diag::DiagnosticsSink* sink);

}  // namespace sem::passes

#endif