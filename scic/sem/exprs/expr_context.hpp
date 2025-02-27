#ifndef SEM_EXPR_CONTEXT_HPP
#define SEM_EXPR_CONTEXT_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/types/choice.hpp"

namespace sem {

class Loop;

class ExprEnvironment {
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

  struct Sym : public util::ChoiceBase<Sym, PropSym, VarSym> {
    using ChoiceBase::ChoiceBase;
  };

  struct LocalProc {
    NameToken name;
    codegen::PtrRef* proc_ref;
  };

  struct ExternProc {
    NameToken name;
    ScriptNum script_num;
    std::size_t extern_offset;
  };

  struct KernelProc {
    NameToken name;
    std::size_t kernel_offset;
  };

  struct Proc
      : public util::ChoiceBase<Proc, LocalProc, ExternProc, KernelProc> {
    using ChoiceBase::ChoiceBase;
  };
  struct SuperInfo {
    ClassSpecies species;
    NameToken super_name;
  };

  virtual ~ExprEnvironment() = default;

  virtual std::optional<SuperInfo> GetSuperInfo() const = 0;

  virtual std::optional<SelectorNum> LookupSelector(
      std::string_view name) const = 0;

  virtual std::optional<Sym> LookupSym(std::string_view name) const = 0;

  virtual Proc const* LookupProc(std::string_view name) const = 0;
};

class ExprContext {
 public:
  ExprContext(codegen::CodeGenerator* codegen,
              codegen::FunctionBuilder* func_builder,
              ClassTable const* class_table,
              SelectorTable const* selector_table,
              std::optional<SuperInfo> super_info,
              std::map<std::string_view, Sym, std::less<>> symbols,
              std::map<std::string_view, Proc, std::less<>> procs)
      : codegen_(codegen),
        func_builder_(func_builder),
        class_table_(class_table),
        selector_table_(selector_table),
        super_info_(std::move(super_info)),
        symbols_(std::move(symbols)),
        procs_(std::move(procs)) {}

  virtual ~ExprContext() = default;

  codegen::CodeGenerator* codegen() const { return codegen_; }
  codegen::FunctionBuilder* func_builder() const { return func_builder_; }
  SelectorTable const* selector_table() const { return selector_table_; }
  std::optional<SuperInfo> const& super_info() const { return super_info_; }

  Sym const* Lookup(std::string_view name) const {
    auto it = symbols_.find(name);
    if (it == symbols_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  Proc const* LookupProc(std::string_view name) const {
    auto it = procs_.find(name);
    if (it == procs_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  codegen::LabelRef* GetContLabel(std::size_t num_levels) const;
  codegen::LabelRef* GetBreakLabel(std::size_t num_levels) const;

  virtual absl::Status BuildExpr(ast::Expr const& expr) const = 0;

 private:
  friend class Loop;

  Loop const* FindLoop(std::size_t at_level) const;

  codegen::CodeGenerator* codegen_;
  codegen::FunctionBuilder* func_builder_;
  ClassTable const* class_table_;
  SelectorTable const* selector_table_;
  std::optional<SuperInfo> super_info_;
  std::map<std::string_view, Sym, std::less<>> symbols_;
  std::map<std::string_view, Proc, std::less<>> procs_;
  mutable Loop const* loop_ = nullptr;
};

class Loop {
 public:
  explicit Loop(ExprContext const* ctx, codegen::LabelRef* cont_label,
                codegen::LabelRef* break_label)
      : ctx_(ctx),
        prev_(ctx->loop_),
        cont_label_(cont_label),
        break_label_(break_label) {
    ctx_->loop_ = this;
  }

  Loop const* prev() const { return prev_; }
  codegen::LabelRef* cont_label() const { return cont_label_; }
  codegen::LabelRef* break_label() const { return break_label_; }

  ~Loop() noexcept(false) {
    if (ctx_->loop_ != this) {
      throw std::logic_error("Loop destructor called in non-nesting order");
    }
    ctx_->loop_ = prev_;
  }

 private:
  ExprContext const* ctx_;
  Loop const* prev_;
  codegen::LabelRef* cont_label_;
  codegen::LabelRef* break_label_;
};

Loop const* ExprContext::FindLoop(std::size_t at_level) const {
  auto const* curr_loop = loop_;
  while (at_level > 0) {
    at_level--;
    if (curr_loop->prev() == nullptr) {
      return curr_loop;
    }
  }
  return curr_loop;
}

codegen::LabelRef* ExprContext::GetContLabel(std::size_t at_level) const {
  auto const* loop = FindLoop(at_level);
  if (!loop) {
    return nullptr;
  }
  return loop->cont_label();
}

codegen::LabelRef* ExprContext::GetBreakLabel(std::size_t at_level) const {
  auto const* loop = FindLoop(at_level);
  if (!loop) {
    return nullptr;
  }
  return loop->break_label();
}

}  // namespace sem

#endif