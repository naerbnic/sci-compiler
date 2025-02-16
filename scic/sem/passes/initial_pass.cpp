#include "scic/sem/passes/initial_pass.hpp"

#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "scic/diagnostics/diagnostics.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/symbol.hpp"

namespace sem::passes {
namespace {

template <class C>
auto GetSingleton(C&& value) {
  auto begin_it = std::begin(value);
  auto end_it = std::end(value);
  if (begin_it == end_it) {
    throw std::runtime_error("Expected exactly one element. Got zero");
  }

  // We want to handle the case when the value is a reference, and when it's
  // a value type.
  using ValueType = decltype(*begin_it);

  // This should get and keep a forwarding reference to the value.
  auto&& result = *begin_it;
  ++begin_it;
  if (begin_it != end_it) {
    throw std::runtime_error("Expected exactly one element. Got more than one");
  }

  return std::forward<ValueType>(result);
}

namespace sci = parsers::sci;

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

ItemRefs ExtractItemRefs(absl::Span<parsers::sci::Item const> items) {
  ItemRefs refs;

  for (auto const& item : items) {
    item.visit(
        [&](sci::ScriptNumDef const& item) {
          refs.script_nums.push_back(&item);
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
          }
        },
        [&](sci::GlobalDeclDef const& item) {
          for (auto const& entry : item.entries()) {
            refs.global_decls.push_back(&entry);
          }
        },
        [&](sci::ModuleVarsDef const& item) {
          refs.module_vars.push_back(&item);
          for (auto const& entry : item.entries()) {
            refs.module_vars_entries.push_back(&entry);
          }
        },
        [&](sci::ProcDef const& item) { refs.procedures.push_back(&item); },
        [&](sci::ClassDef const& item) { refs.class_defs.push_back(&item); },
        [&](sci::ClassDecl const& item) { refs.class_decls.push_back(&item); },
        [&](sci::SelectorsDecl const& item) {
          for (auto const& entry : item.selectors()) {
            refs.selectors.push_back(&entry);
          }
        });
  }

  return refs;
}

ModuleEnvironment RunInitialPass(ItemRefs const& items,
                                 diag::DiagnosticsSink* sink) {
  ModuleEnvironment state;

  if (items.script_nums.size() > 1) {
    sink->Error("Multiple script number definitions.");
  } else if (items.script_nums.empty()) {
    sink->Error("Missing script number definition.");
  } else {
    state.script_num = items.script_nums.front()->script_num().value();
  }

  for (auto const* entry : items.externs) {
    auto sym_id = SymbolId::Create(entry->name.value());
    auto index = entry->index.value();
    auto module_id = entry->module_num.value();

    // Maybe we should collect all of the values before we do error
    // resolution.
    if (auto it = state.extern_decls.find(sym_id);
        it != state.extern_decls.end()) {
      if (it->second.module_id != module_id || it->second.index != index) {
        sink->Error("Duplicate external declaration with mismatching indexes.");
      }
      continue;
    }
    state.extern_decls.emplace(std::move(sym_id), ExternIndex{
                                                      .module_id = module_id,
                                                      .index = index,
                                                  });
  }

  for (auto const* entry : items.global_decls) {
    // We have to parse the vardef, to get the name and the size.
    auto index = entry->index.value();
    auto [sym_id, range] = ParseVarDef(entry->name, index);

    if (state.global_decls.contains(sym_id)) {
      sink->Error("Duplicate global declaration.");
      continue;
    }

    state.global_decls.emplace(std::move(sym_id), index);
  }

  if (items.module_vars.size() > 1) {
    sink->Error("Duplicate module variable declaration.");
  } else if (!items.module_vars.empty()) {
    auto const* module_vars = GetSingleton(items.module_vars);

    if (state.script_num == 0) {
      if (module_vars->kind() != sci::ModuleVarsDef::GLOBAL) {
        sink->Error("Only global module variables are allowed in the kernel.");
      }
    } else if (state.script_num) {
      if (module_vars->kind() != sci::ModuleVarsDef::LOCAL) {
        sink->Error(
            "Only local module variables are allowed in "
            "non-kernel scripts.");
      }
    }

    for (auto const* entry : items.module_vars_entries) {
      auto index = entry->index.value();
      auto [sym_id, range] = ParseVarDef(entry->name, index);

      if (state.var_decls.contains(sym_id)) {
        sink->Error("Duplicate module variable declaration.");
        continue;
      }

      state.var_decls.emplace(std::move(sym_id), range);
    }
  }

  for (auto const* entry : items.procedures) {
    auto sym_id = SymbolId::Create(entry->name().value());
    if (state.procedures.contains(sym_id)) {
      sink->Error("Duplicate procedure definition.");
      continue;
    } else {
      state.procedures.emplace(std::move(sym_id), ProcInfo{});
    }
  }

  for (auto const* entry : items.class_defs) {
    auto sym_id = SymbolId::Create(entry->name().value());
    if (state.class_defs.contains(sym_id)) {
      sink->Error("Duplicate class definition.");
      continue;
    }
    state.class_defs.emplace(std::move(sym_id), ClassDefInfo{
                                                    .kind = entry->kind(),
                                                });
  }

  for (auto const* class_decl : items.class_decls) {
    auto sym_id = SymbolId::Create(class_decl->name().value());
    if (state.class_decls.contains(sym_id)) {
      sink->Error("Duplicate class declaration.");
      continue;
    }
    state.class_decls.emplace(std::move(sym_id), ClassDeclInfo{});
  }

  for (auto const* selector : items.selectors) {
    auto sym_id = SelectorId::Create(selector->name.value());
    auto index = selector->id.value();

    if (state.selector_decls.contains(sym_id)) {
      sink->Error("Duplicate selector declaration.");
      continue;
    }

    state.selector_decls.emplace(std::move(sym_id), index);
  }

  return state;
}
}  // namespace sem::passes