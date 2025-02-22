#ifndef SEM_PASSES_BUILD_CODE_HPP
#define SEM_PASSES_BUILD_CODE_HPP

#include <memory>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/selector_table.hpp"

namespace sem::passes {

// Gets the script ID from the module with the given items.
absl::StatusOr<ScriptNum> GetScriptId(Items items);

// Builds a new SelectorTable from the given items.
//
// The resulting table includes both the selectors previously declared in the
// module, and any new selectors that were added in class definitions.
absl::StatusOr<std::unique_ptr<SelectorTable>> BuildSelectorTableFromItems(
    absl::Span<ast::Item const> items);

// Collects all the class definitions from a module.
absl::StatusOr<std::unique_ptr<ClassTable>> BuildClassTable(
    SelectorTable const* sel_table, ScriptNum script_num,
    absl::Span<ast::Item const> items);

};  // namespace sem::passes
#endif