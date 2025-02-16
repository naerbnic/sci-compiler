#include "scic/sem/passes/initial_pass.hpp"

#include <map>
#include <optional>
#include <utility>

#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/symbol.hpp"

namespace sem::passes {
namespace {

namespace sci = parsers::sci;

// A range of variable indexes.
//
// A simple variable has a single index, but an array has a range of indexes.
// This represents the range [start, end).
struct IndexRange {
  int start;
  int end;
};

struct ModuleVars {
  parsers::sci::ModuleVarsDef::Kind kind;
  std::map<SymbolId, IndexRange> var_decls;
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

struct PassState {
  std::optional<int> script_num;
  std::map<SymbolId, int> global_decls;
  std::optional<ModuleVars> module_vars;
  std::map<SymbolId, ExternIndex> extern_decls;
  std::map<SelectorId, int> selectors;
  std::map<SymbolId, ClassDeclInfo> class_decls;
  std::map<SymbolId, ClassDefInfo> class_defs;
  std::map<SymbolId, ProcInfo> procedures;
};

std::pair<SymbolId, IndexRange> ParseVarDef(sci::VarDef const& var,
                                            int base_index) {
  return var.visit(
      [&](sci::SingleVarDef const& var) {
        return std::make_pair(SymbolId::Create(var.name().value()),
                              IndexRange{base_index, base_index + 1});
      },
      [&](sci::ArrayVarDef const& var) {
        return std::make_pair(
            SymbolId::Create(var.name().value()),
            IndexRange{base_index, base_index + var.size().value()});
      });
}

}  // namespace

ModuleEnvironment RunInitialPass(absl::Span<parsers::sci::Item const> items,
                                 diag::DiagnosticsSink* sink) {
  PassState state;

  for (auto const& item : items) {
    item.visit(
        [&](sci::ScriptNumDef const& item) {
          if (state.script_num) {
            sink->Error("Duplicate script number definition.");
            return;
          }
          state.script_num = item.script_num().value();
        },
        [&](sci::PublicDef const& item) {
          // No need to do anything right now.
        },
        [&](sci::ExternDef const& item) {
          for (auto const& entry : item.entries()) {
            auto sym_id = SymbolId::Create(entry.name.value());
            auto index = entry.index.value();
            auto module_id = entry.module_num.value();

            // Maybe we should collect all of the values before we do error
            // resolution.
            if (auto it = state.extern_decls.find(sym_id);
                it != state.extern_decls.end()) {
              if (it->second.module_id != module_id ||
                  it->second.index != index) {
                sink->Error(
                    "Duplicate external declaration with mismatching indexes.");
              }
              continue;
            }
            state.extern_decls.emplace(std::move(sym_id),
                                       ExternIndex{
                                           .module_id = module_id,
                                           .index = index,
                                       });
          }
        },
        [&](sci::GlobalDeclDef const& item) {
          for (auto const& entry : item.entries()) {
            // We have to parse the vardef, to get the name and the size.
            auto index = entry.index.value();
            auto [sym_id, range] = ParseVarDef(entry.name, index);

            if (state.global_decls.contains(sym_id)) {
              sink->Error("Duplicate global declaration.");
              continue;
            }

            state.global_decls.emplace(std::move(sym_id), index);
          }
        },
        [&](sci::ModuleVarsDef const& item) {
          if (state.module_vars) {
            sink->Error("Duplicate module variable declaration.");
            return;
          }

          ModuleVars module_vars{
              .kind = item.kind(),
          };

          for (auto const& entry : item.entries()) {
            auto index = entry.index.value();
            auto [sym_id, range] = ParseVarDef(entry.name, index);

            if (module_vars.var_decls.contains(sym_id)) {
              sink->Error("Duplicate module variable declaration.");
              continue;
            }

            module_vars.var_decls.emplace(std::move(sym_id), range);
          }
        },
        [&](sci::ProcDef const& item) {
          auto sym_id = SymbolId::Create(item.name().value());
          if (state.procedures.contains(sym_id)) {
            sink->Error("Duplicate procedure definition.");
            return;
          }
          state.procedures.emplace(std::move(sym_id), ProcInfo{});
        },
        [&](sci::ClassDef const& item) {
          auto sym_id = SymbolId::Create(item.name().value());
          if (state.class_defs.contains(sym_id)) {
            sink->Error("Duplicate class definition.");
            return;
          }
          state.class_defs.emplace(std::move(sym_id), ClassDefInfo{
                                                          .kind = item.kind(),
                                                      });
        },
        [&](sci::ClassDecl const& item) {
          auto sym_id = SymbolId::Create(item.name().value());
          if (state.class_decls.contains(sym_id)) {
            sink->Error("Duplicate class declaration.");
            return;
          }
          state.class_decls.emplace(std::move(sym_id), ClassDeclInfo{});
        },
        [&](sci::SelectorsDecl const& item) {
          for (auto const& entry : item.selectors()) {
            auto sym_id = SelectorId::Create(entry.name.value());
            auto index = entry.id.value();

            if (state.selectors.contains(sym_id)) {
              sink->Error("Duplicate selector declaration.");
              continue;
            }

            state.selectors.emplace(std::move(sym_id), index);
          }
        });
  }

  if (!state.script_num) {
    sink->Error("Missing (script# ...)");
  }

  return ModuleEnvironment{};
}
}  // namespace sem::passes