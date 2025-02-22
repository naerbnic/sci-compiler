#ifndef SEM_EXPR_CONTEXT_HPP
#define SEM_EXPR_CONTEXT_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/types/choice.hpp"

namespace sem {

class ExprContext {
 public:
  struct ParamSym {
    std::size_t param_offset;
  };

  struct TempSym {
    std::size_t temp_offset;
  };

  struct GlobalSym {
    std::size_t global_offset;
  };

  struct LocalSym {
    std::size_t local_offset;
  };

  struct VarSym : public util::ChoiceBase<VarSym, ParamSym, GlobalSym, LocalSym,
                                          TempSym> {
    using ChoiceBase::ChoiceBase;
  };

  struct PropSym {
    std::size_t prop_offset;
    SelectorTable::Entry const* selector;
  };

  struct ProcSym {
    codegen::PtrRef* proc_ref;
  };

  struct Sym : public util::ChoiceBase<Sym, PropSym, VarSym> {
    using ChoiceBase::ChoiceBase;
  };

  ExprContext(codegen::CodeGenerator* codegen,
              codegen::FunctionBuilder* func_builder,
              ClassTable const* class_table,
              SelectorTable const* selector_table,
              std::map<std::string_view, Sym, std::less<>> symbols)
      : codegen_(codegen),
        func_builder_(func_builder),
        class_table_(class_table),
        selector_table_(selector_table),
        symbols_(std::move(symbols)) {}

  codegen::CodeGenerator* codegen() const { return codegen_; }
  codegen::FunctionBuilder* func_builder() const { return func_builder_; }
  ClassTable const* class_table() const { return class_table_; }
  SelectorTable const* selector_table() const { return selector_table_; }

  Sym const* Lookup(std::string_view name) const {
    auto it = symbols_.find(name);
    if (it == symbols_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  virtual absl::Status BuildExpr(ast::Expr const& expr) const = 0;

 private:
  codegen::CodeGenerator* codegen_;
  codegen::FunctionBuilder* func_builder_;
  ClassTable const* class_table_;
  SelectorTable const* selector_table_;
  std::map<std::string_view, Sym, std::less<>> symbols_;
};

}  // namespace sem

#endif