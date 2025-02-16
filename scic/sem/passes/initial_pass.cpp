#include "scic/sem/passes/initial_pass.hpp"

#include <map>
#include <optional>
#include <utility>
#include <vector>

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

struct ItemRefs {
  std::vector<sci::ScriptNumDef const*> script_nums;
  std::vector<sci::PublicDef const*> publics;
  std::vector<sci::PublicDef::Entry const*> public_entries;
  std::vector<sci::ExternDef::Entry const*> externs;
  std::vector<sci::GlobalDeclDef::Entry const*> global_decls;
  std::vector<sci::ModuleVarsDef const*> module_vars;
  std::vector<sci::ModuleVarsDef::Entry const*> module_vars_entries;
  std::vector<sci::ProcDef const*> procedures;
  std::vector<sci::ClassDef const*> class_defs;
  std::vector<sci::ClassDecl const*> class_decls;
  std::vector<sci::SelectorsDecl::Entry const*> selectors;
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
  ItemRefs refs;

  for (auto const& item : items) {
    item.visit(
        [&](sci::ScriptNumDef const& item) {
          refs.script_nums.push_back(&item);
          if (state.script_num) {
            sink->Error("Duplicate script number definition.");
            return;
          }
          state.script_num = item.script_num().value();
        },
        [&](sci::PublicDef const& item) {
          refs.publics.push_back(&item);
          for (auto const& entry : item.entries()) {
            refs.public_entries.push_back(&entry);
          }
        },
        [&](sci::ExternDef const& item) {
          for (auto const& entry : item.entries()) {
            refs.externs.push_back(&entry);
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
            refs.global_decls.push_back(&entry);
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
          refs.module_vars.push_back(&item);
          if (state.module_vars) {
            sink->Error("Duplicate module variable declaration.");
            return;
          }

          ModuleVars module_vars{
              .kind = item.kind(),
          };

          for (auto const& entry : item.entries()) {
            refs.module_vars_entries.push_back(&entry);
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
          refs.procedures.push_back(&item);
          auto sym_id = SymbolId::Create(item.name().value());
          if (state.procedures.contains(sym_id)) {
            sink->Error("Duplicate procedure definition.");
            return;
          }
          state.procedures.emplace(std::move(sym_id), ProcInfo{});
        },
        [&](sci::ClassDef const& item) {
          refs.class_defs.push_back(&item);
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
          refs.class_decls.push_back(&item);
          auto sym_id = SymbolId::Create(item.name().value());
          if (state.class_decls.contains(sym_id)) {
            sink->Error("Duplicate class declaration.");
            return;
          }
          state.class_decls.emplace(std::move(sym_id), ClassDeclInfo{});
        },
        [&](sci::SelectorsDecl const& item) {
          for (auto const& entry : item.selectors()) {
            refs.selectors.push_back(&entry);
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

  std::optional<int> script_num;

  if (refs.script_nums.size() > 1) {
    sink->Error("Multiple script number definitions.");
  } else if (refs.script_nums.empty()) {
    sink->Error("Missing script number definition.");
  } else {
    script_num = refs.script_nums.front()->script_num().value();
  }

  

  return ModuleEnvironment{};
}
}  // namespace sem::passes