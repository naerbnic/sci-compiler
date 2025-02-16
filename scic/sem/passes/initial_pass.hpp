#ifndef SEM_PASSES_INITIAL_PASS_HPP
#define SEM_PASSES_INITIAL_PASS_HPP

#include <map>
#include <vector>

#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/symbol.hpp"

namespace sem::passes {

struct ItemRefs {
  std::vector<parsers::sci::ScriptNumDef const*> script_nums;
  std::vector<parsers::sci::PublicDef const*> publics;
  std::vector<parsers::sci::PublicDef::Entry const*> public_entries;
  std::vector<parsers::sci::ExternDef::Entry const*> externs;
  std::vector<parsers::sci::GlobalDeclDef::Entry const*> global_decls;
  std::vector<parsers::sci::ModuleVarsDef const*> module_vars;
  std::vector<parsers::sci::ModuleVarsDef::Entry const*> module_vars_entries;
  std::vector<parsers::sci::ProcDef const*> procedures;
  std::vector<parsers::sci::ClassDef const*> class_defs;
  std::vector<parsers::sci::ClassDecl const*> class_decls;
  std::vector<parsers::sci::SelectorsDecl::Entry const*> selectors;
};

ItemRefs ExtractItemRefs(absl::Span<parsers::sci::Item const> items);

// A range of variable indexes.
//
// A simple variable has a single index, but an array has a range of indexes.
// This represents the range [start, end).
struct IndexRange {
  int start;
  int end;
};

struct ExternIndex {
  int module_id;
  int index;
};

struct ClassDeclInfo {};

struct ClassDefInfo {
  parsers::sci::ClassDef::Kind kind;
};

struct ProcInfo {};

struct ModuleEnvironment {
  int script_num;
  std::map<SymbolId, int> global_decls;
  std::map<SymbolId, IndexRange> var_decls;
  std::map<SymbolId, ExternIndex> extern_decls;
  std::map<SelectorId, int> selector_decls;
  std::map<SymbolId, ClassDeclInfo> class_decls;
  std::map<SymbolId, ClassDefInfo> class_defs;
  std::map<SymbolId, ProcInfo> procedures;
};

ModuleEnvironment RunInitialPass(ItemRefs const& items,
                                 diag::DiagnosticsSink* sink);

}  // namespace sem::passes

#endif